// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// A guard on the USA game's named memory pool, against the freeze after a
// race. Hang report 2026-10-21_13-21-34 (vivo X300 Pro, back to the menu
// after a race): every guest thread waited except one, running inside
// sub_8215E5E8 (ctx.lr 8215E714) for 20 s while the GPU sat idle and nothing
// was read from the disc.
//
// sub_8215E5E8 looks a name up in the pool whose header is at 0x82A2E0E0
// (+0 its size in 8-byte units, +8 its base, +12 a critical section, +10300
// a rover) and otherwise takes a free block. The blocks tile the pool: each
// starts with its length in 8-byte units (u16 at +0) and an in-use word (u16
// at +4, the name from +6). Both walks step by that length and stop only on
// an exact address: the end of the pool for the name search, the rover for
// the free-block search. A block of length 0, a chain that steps past the
// end, or a rover that isn't on a block's start therefore loops forever,
// which is the hang.
//
// Before each lookup, under the pool's own critical section (recursive, so
// the function takes it again as usual), the chain is walked once. A rover
// off the chain is put back at the pool's base, which the free-block search
// accepts like any other start. A broken chain can't be repaired: the lookup
// returns 0, as the game's own "nothing found" (the function does the same
// for an empty name or pool), instead of never returning. Each problem goes
// to the log ([pool-guard]), with the blocks around it.
//
// Only on the USA default.xex: the guard arms itself when the function's
// words that load the pool's address are the expected ones.
// NFSMW_POOL_GUARD=0 leaves the function alone (an A/B switch).
#include <stdafx.h>

#include <kernel/function.h>

#include <atomic>
#include <cstdio>

extern "C" PPC_FUNC(__imp__sub_8215E5E8);
extern "C" PPC_FUNC(__imp__sub_828129D0);  // RtlEnterCriticalSection(r3 + 4)
extern "C" PPC_FUNC(__imp__sub_828129D8);  // RtlLeaveCriticalSection(r3 + 4)

namespace
{
    constexpr uint32_t kPool = 0x82A2E0E0;      // lis r11,-32093; addi r30,r11,-7968
    constexpr uint32_t kRover = kPool + 10300;  // lwz r8,10300(r30)
    constexpr uint32_t kLock = kPool + 12;      // addi r3,r30,12; bl RtlEnterCriticalSection's thunk

    std::atomic<int> s_reports{ 0 };

    // The words of sub_8215E5E8 that build the pool's address (0x8215E600:
    // lis r11,-32093; 0x8215E604: addi r30,r11,-7968).
    bool Armed(uint8_t* base)
    {
        static const bool armed = [base] {
            const char* v = std::getenv("NFSMW_POOL_GUARD");
            if (v && v[0] == '0')
            {
                fprintf(stderr, "[pool-guard] NFSMW_POOL_GUARD=0: off\n");
                return false;
            }
            const bool usa = PPC_LOAD_U32(0x8215E600) == 0x3D6082A3 && PPC_LOAD_U32(0x8215E604) == 0x3BCBE0E0;
            fprintf(stderr, "[pool-guard] named pool %08X: %s\n", kPool,
                usa ? "guarded" : "not the expected code (another executable?): left alone");
            return usa;
        }();
        return armed;
    }

    void Report(uint8_t* base, const char* what, uint32_t at, uint32_t poolBase, uint32_t end, uint32_t rover, uint32_t steps)
    {
        if (s_reports.fetch_add(1) >= 8)
            return;
        fprintf(stderr, "[pool-guard] %s at %08X (pool %08X-%08X, rover %08X, %u blocks walked)\n",
            what, at, poolBase, end, rover, steps);
        // The block there and the next few words, for the log.
        if (at >= poolBase && at + 16 <= end)
            fprintf(stderr, "[pool-guard]   %08X: %08X %08X %08X %08X\n", at,
                PPC_LOAD_U32(at), PPC_LOAD_U32(at + 4), PPC_LOAD_U32(at + 8), PPC_LOAD_U32(at + 12));
    }
}

PPC_FUNC(sub_8215E5E8)
{
    const uint32_t name = ctx.r4.u32;
    // The function's own early outs (no name, no pool) need no walk.
    if (!name || !Armed(base) || !PPC_LOAD_U32(kPool + 8))
    {
        __imp__sub_8215E5E8(ctx, base);
        return;
    }
    const uint64_t r3 = ctx.r3.u64, r4 = ctx.r4.u64;
    ctx.r3.u64 = kLock;
    __imp__sub_828129D0(ctx, base);  // RtlEnterCriticalSection(pool + 12)

    const uint32_t units = PPC_LOAD_U32(kPool), poolBase = PPC_LOAD_U32(kPool + 8);
    const uint32_t end = poolBase + units * 8, rover = PPC_LOAD_U32(kRover);
    bool roverOnChain = false, broken = false;
    uint32_t at = poolBase, steps = 0;
    while (at != end)
    {
        if (at == rover)
            roverOnChain = true;
        const uint32_t length = PPC_LOAD_U16(at);
        if (length == 0)
        {
            Report(base, "block of length 0", at, poolBase, end, rover, steps);
            broken = true;
            break;
        }
        const uint32_t next = at + length * 8;
        if (next > end || next < at || ++steps > units)
        {
            Report(base, "chain runs past the end", at, poolBase, end, rover, steps);
            broken = true;
            break;
        }
        at = next;
    }
    if (!broken && !roverOnChain && units)
    {
        Report(base, "rover off the chain; reset to the base", rover, poolBase, end, rover, steps);
        PPC_STORE_U32(kRover, poolBase);
    }

    ctx.r3.u64 = kLock;
    __imp__sub_828129D8(ctx, base);  // RtlLeaveCriticalSection
    ctx.r3.u64 = r3;
    ctx.r4.u64 = r4;
    if (broken)
    {
        ctx.r3.u64 = 0;  // "nothing found", rather than a lookup that never returns
        return;
    }
    __imp__sub_8215E5E8(ctx, base);
}
