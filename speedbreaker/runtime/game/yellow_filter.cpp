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
    constexpr uint32_t kPhysicalFirst = 0xA0000000, kPhysicalEnd = 0xC0000000;

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
        const bool virtualRange = address >= kFirstGuest && uint64_t(address) + bytes <= kEndGuest;
        const bool physicalRange = address >= kPhysicalFirst && uint64_t(address) + bytes <= kPhysicalEnd;
        if ((address & 3) || !(virtualRange || physicalRange))
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

    float FromBits(uint32_t hostBits)
    {
        const uint32_t bits = ByteSwap(hostBits);
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }

    bool NearFloat(const uint32_t* words, size_t index, size_t count, float v)
    {
        for (int d = -6; d <= 9; d++)
        {
            const int64_t j = int64_t(index) + d;
            if (j >= 0 && size_t(j) < count && std::fabs(FromBits(words[j]) - v) < 1e-6f)
                return true;
        }
        return false;
    }

    // Background: every committed read/write region of the virtual heaps and of
    // the physical window (where the game keeps much of its loaded data).
    void Scan()
    {
        struct Known { const char* name; uint32_t bits[3]; };
        const Known known[] = {
            { "PC default 0.88 0.80 0.44", { BigEndianBits(0.88f), BigEndianBits(0.80f), BigEndianBits(0.44f) } },
            { "X360-like 0.68 0.58 0.03", { BigEndianBits(0.68f), BigEndianBits(0.58f), BigEndianBits(0.03f) } },
            { "Diamond 0.90 1.00 0.46", { BigEndianBits(0.90f), BigEndianBits(1.00f), BigEndianBits(0.46f) } },
        };
        const uint32_t pair[2] = { BigEndianBits(0.9f), BigEndianBits(1.75f) };
        uint64_t bytes = 0;
        uint32_t exactFound = 0, pairFound = 0, candidates = 0;
        static uint32_t candidateLogged = 0;
        const uint32_t ranges[][2] = { { kFirstGuest, kEndGuest }, { kPhysicalFirst, kPhysicalEnd } };
        for (const auto& range : ranges)
        for (uint64_t at = range[0]; at < range[1];)
        {
            vmem::PageHeap* heap = vmem::HeapFor(uint32_t(at));
            vmem::RegionInfo info{};
            if (!heap || !heap->Query(uint32_t(at), &info) || info.regionSize == 0)
            {
                at += 0x10000;
                continue;
            }
            const uint64_t end = std::min<uint64_t>(uint64_t(info.baseAddress) + info.regionSize, range[1]);
            if (info.state == vmem::X_MEM_COMMIT && (info.protect & (0x04 | 0x40)) && info.baseAddress >= range[0] && end > info.baseAddress + 16)
            {
                const uint32_t* words = static_cast<const uint32_t*>(g_memory.Translate(info.baseAddress));
                const size_t count = size_t(end - info.baseAddress) / 4;
                bytes += count * 4;
                for (size_t i = 0; i + 3 <= count; i++)
                {
                    const uint32_t first = ByteSwap(words[i]);
                    // Floats from 0.5 to a little over 1 only (cheap reject).
                    if (first < 0x3F000000u || first > 0x3F866666u)
                        continue;
                    const uint32_t address = info.baseAddress + uint32_t(i * 4);
                    const char* hit = nullptr;
                    for (const Known& k : known)
                        if (words[i] == k.bits[0] && words[i + 1] == k.bits[1] && words[i + 2] == k.bits[2])
                            hit = k.name;
                    if (hit)
                    {
                        exactFound++;
                        Remember(hit, address);
                        continue;
                    }
                    const float r = FromBits(words[i]), g = FromBits(words[i + 1]), b = FromBits(words[i + 2]);
                    if (!game::visual_filter::TintShape(r, g, b))
                        continue;
                    // A tint-shaped triple beside the bloom numbers (the PC's 0.9 / 1.75, the community's 1.2 / 1.8 / 2.5 / 0.7).
                    const bool bloom = NearFloat(words, i, count, 1.75f) || NearFloat(words, i, count, 1.2f) || NearFloat(words, i, count, 0.7f) ||
                        NearFloat(words, i, count, 1.8f) || NearFloat(words, i, count, 2.5f);
                    if (i >= 2 && words[i - 2] == pair[0] && words[i - 1] == pair[1])
                    {
                        pairFound++;
                        Remember("after blackbloom 0.9, colorbloom 1.75", address);
                    }
                    else if (bloom)
                    {
                        candidates++;
                        if (candidateLogged < 24)
                        {
                            candidateLogged++;
                            LogSpot("candidate: tint-shaped triple beside bloom-like numbers", address);
                        }
                    }
                }
            }
            at = end;
        }
        const uint32_t scan = ++s_scans;
        if (scan <= 4 || exactFound || pairFound)
            fprintf(stderr, "[game-filter] scan %u: %llu MB of guest memory, %u known tint triples, %u colorbloom pairs, %u candidates, %zu spots\n",
                scan, (unsigned long long)(bytes >> 20), exactFound, pairFound, candidates, s_spots.size());
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
