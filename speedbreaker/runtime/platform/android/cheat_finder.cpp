// SpeedBreaker One. GPL-3.0-or-later.
//
// The CHEATS menu's number finder (CheatsMenu.java): the classic way to
// change a value whose address is not known, such as the career's money.
// Search guest memory for the number shown in the game, change it in the
// game, search the matches again for the new number, until one or a few
// addresses are left; then write the wanted number there.
//
// Values are 32-bit big-endian integers at 4-byte-aligned guest addresses.
// Searched: the game's virtual heaps (0x00010000-0x7EFFFFFF; the runtime's
// own objects above are left out) and the physical window
// (0xA0000000-0xBFFFFFFF), the layout in kernel/vmem.h. All of it is mapped
// readable (kernel/memory.cpp) except where the write watch makes a page
// PROT_NONE while the GPU writes it: a read there faults into the process's
// handler (main.cpp, writewatch::HandleFault), which waits for the GPU and
// opens the page, as for a game read. At worst a hitch, never a crash.
//
// Called from a Java background thread; one search at a time.
#include <stdafx.h>
#include <jni.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>

namespace
{
    struct Range { uint32_t begin, end; };
    constexpr Range kRanges[] = {
        { 0x00010000, 0x40000000 },
        { 0x40000000, 0x7F000000 },
        { 0xA0000000, 0xC0000000 },
    };
    // More matches than this after the first search: the number is too
    // common (0, 1, 100...) to narrow down usefully; the rest are dropped.
    constexpr size_t kMaxMatches = 4u << 20;

    std::mutex s_mutex;
    std::vector<uint32_t> s_matches;
    bool s_truncated = false;

    uint32_t ReadGuest(uint32_t address)
    {
        return ByteSwap(*static_cast<volatile uint32_t*>(g_memory.Translate(address)));
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_onenonlygit_speedbreakerone_CheatsMenu_nativeFindFirst(JNIEnv*, jclass, jint value)
{
    std::lock_guard lock(s_mutex);
    s_matches.clear();
    s_truncated = false;
    const uint32_t wanted = uint32_t(value);
    for (const Range& r : kRanges)
        for (uint32_t a = r.begin; a < r.end; a += 4)
            if (ReadGuest(a) == wanted)
            {
                if (s_matches.size() >= kMaxMatches)
                {
                    s_truncated = true;
                    break;
                }
                s_matches.push_back(a);
            }
    fprintf(stderr, "[cheats] finder: %zu addresses hold %u%s\n", s_matches.size(), wanted,
        s_truncated ? " (more not kept: too common a number)" : "");
    return jint(s_matches.size());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_onenonlygit_speedbreakerone_CheatsMenu_nativeFindNext(JNIEnv*, jclass, jint value)
{
    std::lock_guard lock(s_mutex);
    const uint32_t wanted = uint32_t(value);
    std::vector<uint32_t> kept;
    for (uint32_t a : s_matches)
        if (ReadGuest(a) == wanted)
            kept.push_back(a);
    s_matches.swap(kept);
    fprintf(stderr, "[cheats] finder: %zu of the matches now hold %u\n", s_matches.size(), wanted);
    return jint(s_matches.size());
}

// Writes `value` at every remaining match, if there are at most `limit`:
// more would mean writing over things that only happen to hold the number.
extern "C" JNIEXPORT jint JNICALL
Java_com_onenonlygit_speedbreakerone_CheatsMenu_nativeSetMatches(JNIEnv*, jclass, jint value, jint limit)
{
    std::lock_guard lock(s_mutex);
    if (s_matches.empty() || s_matches.size() > size_t(limit))
        return 0;
    for (uint32_t a : s_matches)
    {
        *static_cast<volatile uint32_t*>(g_memory.Translate(a)) = ByteSwap(uint32_t(value));
        fprintf(stderr, "[cheats] finder: %08X set to %d\n", a, int(value));
    }
    return jint(s_matches.size());
}
