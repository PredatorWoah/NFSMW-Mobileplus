// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <cstdint>

namespace gpu
{
    // Compare uses four 120-frame reporting windows per arm. The first
    // window after a switch is a transition, not a steady measurement.
    constexpr uint32_t AndroidBarrierLevel(int mode, uint64_t frame)
    {
        return mode == 2 ? 2u : mode == 4 && ((frame ? frame - 1 : 0) / 480) % 2 ? 2u : 0u;
    }

    constexpr uint64_t TimestampTicks(uint64_t begin, uint64_t end, uint32_t bits)
    {
        return bits == 0 ? 0 : bits >= 64 ? end - begin : (end - begin) & ((uint64_t(1) << bits) - 1);
    }
}
