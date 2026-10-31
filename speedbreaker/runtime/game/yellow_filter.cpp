// SpeedBreaker One. GPL-3.0-or-later. See yellow_filter.h.
#include <stdafx.h>
#include "yellow_filter.h"
#include <kernel/vmem.h>
#include <user/settings.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    constexpr size_t kMaxSpots = 64;
    constexpr uint32_t kScanEveryFrames = 300;  // about 5 s at 60 fps
    constexpr uint32_t kFirstGuest = 0x00010000, kEndGuest = 0x7F000000;

    struct Spot
    {
        uint32_t address;      // the red float; green and blue follow
        float originalBlue;
        float written;         // the blue we last wrote (0: none yet)
    };
    std::mutex s_mutex;
    std::vector<Spot> s_spots;
    std::atomic<bool> s_scanning{ false };
    std::atomic<uint32_t> s_scans{ 0 };
    bool s_wasOn = false;
    uint32_t s_frames = 0;

    uint32_t BigEndianBits(float f)
    {
        uint32_t bits;
        memcpy(&bits, &f, 4);
        return ByteSwap(bits);
    }

    float ReadFloat(uint32_t address)
    {
        const uint32_t bits = ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(address)));
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }

    void WriteFloat(uint32_t address, float f)
    {
        uint32_t bits;
        memcpy(&bits, &f, 4);
        *static_cast<uint32_t*>(g_memory.Translate(address)) = ByteSwap(bits);
    }

    // [address, address + bytes) committed and writable.
    bool Usable(uint32_t address, uint32_t bytes)
    {
        if (address < kFirstGuest || (address & 3) || uint64_t(address) + bytes > kEndGuest)
            return false;
        vmem::PageHeap* heap = vmem::HeapFor(address);
        vmem::RegionInfo info{};
        return heap && heap->Query(address, &info) && info.state == vmem::X_MEM_COMMIT && (info.protect & (0x04 | 0x40)) &&
            uint64_t(address) + bytes <= uint64_t(info.baseAddress) + info.regionSize;
    }

    void LogSpot(const char* how, uint32_t address)
    {
        // The floats around it, so the structure shows in the log.
        std::string around;
        for (int i = -8; i < 8; i++)
        {
            const uint32_t a = address + uint32_t(i * 4);
            around += Usable(a, 4) ? std::format(" {:.3f}", ReadFloat(a)) : " ?";
            if (i == -1)
                around += " |";
        }
        fprintf(stderr, "[game-filter] %s at %08X: tint %.3f %.3f %.3f; floats around (red at |):%s\n", how, address, ReadFloat(address),
            ReadFloat(address + 4), ReadFloat(address + 8), around.c_str());
    }

    void Remember(const char* how, uint32_t address)
    {
        std::lock_guard lock(s_mutex);
        for (const Spot& s : s_spots)
            if (s.address == address)
                return;
        if (s_spots.size() >= kMaxSpots)
            return;
        s_spots.push_back({ address, ReadFloat(address + 8), 0.0f });
        LogSpot(how, address);
    }

    // Background: every committed read/write region of the two virtual heaps.
    void Scan()
    {
        const uint32_t exact[3] = { BigEndianBits(0.88f), BigEndianBits(0.80f), BigEndianBits(0.44f) };
        const uint32_t pair[2] = { BigEndianBits(0.9f), BigEndianBits(1.75f) };
        uint64_t bytes = 0;
        uint32_t exactFound = 0, pairFound = 0;
        for (uint64_t at = kFirstGuest; at < kEndGuest;)
        {
            vmem::PageHeap* heap = vmem::HeapFor(uint32_t(at));
            vmem::RegionInfo info{};
            if (!heap || !heap->Query(uint32_t(at), &info) || info.regionSize == 0)
            {
                at += 0x10000;
                continue;
            }
            const uint64_t end = std::min<uint64_t>(uint64_t(info.baseAddress) + info.regionSize, kEndGuest);
            if (info.state == vmem::X_MEM_COMMIT && (info.protect & (0x04 | 0x40)) && info.baseAddress >= kFirstGuest && end > info.baseAddress + 16)
            {
                const uint32_t* words = static_cast<const uint32_t*>(g_memory.Translate(info.baseAddress));
                const size_t count = size_t(end - info.baseAddress) / 4;
                bytes += count * 4;
                for (size_t i = 0; i + 3 <= count; i++)
                {
                    if (words[i] == exact[0] && words[i + 1] == exact[1] && words[i + 2] == exact[2])
                    {
                        exactFound++;
                        Remember("exact 0.88 0.80 0.44", info.baseAddress + uint32_t(i * 4));
                    }
                    else if (words[i] == pair[0] && i + 5 <= count && words[i + 1] == pair[1])
                    {
                        // blackbloom, colorbloom, then the tint if the layout follows the game's debug print order.
                        pairFound++;
                        const uint32_t tint = info.baseAddress + uint32_t(i * 4) + 8;
                        const float r = ReadFloat(tint), g = ReadFloat(tint + 4), b = ReadFloat(tint + 8);
                        if (game::visual_filter::TintShape(r, g, b))
                            Remember("after blackbloom 0.9, colorbloom 1.75", tint);
                        else if (pairFound <= 4)
                            LogSpot("blackbloom 0.9, colorbloom 1.75 (no tint after it)", tint);
                    }
                }
            }
            at = end;
        }
        const uint32_t scan = ++s_scans;
        if (scan <= 3 || exactFound || pairFound)
            fprintf(stderr, "[game-filter] scan %u: %llu MB of guest memory, %u exact tint triples, %u colorbloom pairs, %zu spots\n", scan,
                (unsigned long long)(bytes >> 20), exactFound, pairFound, s_spots.size());
        s_scanning = false;
    }

    // Every frame: keep each spot's blue where it should be.
    void Apply(bool on)
    {
        std::lock_guard lock(s_mutex);
        for (Spot& s : s_spots)
        {
            if (!Usable(s.address, 12))
                continue;
            const float r = ReadFloat(s.address), g = ReadFloat(s.address + 4), b = ReadFloat(s.address + 8);
            if (on)
            {
                if (game::visual_filter::TintShape(r, g, b))
                {
                    const float want = game::visual_filter::NeutralBlue(r, g);
                    WriteFloat(s.address + 8, want);
                    if (s.written == 0.0f)
                        fprintf(stderr, "[game-filter] %08X: blue %.3f -> %.3f (red %.3f, green %.3f)\n", s.address, b, want, r, g);
                    s.written = want;
                }
            }
            else if (s.written != 0.0f && b == s.written)
            {
                WriteFloat(s.address + 8, s.originalBlue);
                s.written = 0.0f;
            }
        }
    }
}

namespace game::visual_filter
{
    void OnFrame()
    {
        const bool on = settings::GetBool(settings::Id::GameYellowFilter);
        if (!on && !s_wasOn)
            return;
        if (on != s_wasOn)
            fprintf(stderr, "[game-filter] %s\n", on ? "on: looking for the game's colour filter" : "off: blue put back");
        s_wasOn = on;
        Apply(on);
        if (on && (s_frames++ % kScanEveryFrames == 0) && !s_scanning.exchange(true))
            std::thread(Scan).detach();
    }
}
