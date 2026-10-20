// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include "android_schedule.h"
#include <cstdint>

namespace gpu::timing
{
    constexpr uint32_t MaxRegions = 256;
    constexpr uint32_t MaxSamples = 120;
    struct Schedule
    {
        uint64_t complexFrames = 0;
        uint32_t samples = 0;
        bool Next(uint64_t draws)
        {
            if (draws < 1500 || samples >= MaxSamples) return false;
            if (++complexFrames % 120 != 30) return false;
            ++samples;
            return true;
        }
    };
    struct Ledger
    {
        uint32_t used = 0, dropped = 0;
        int Add()
        {
            if (used == MaxRegions) { ++dropped; return -1; }
            return int(used++);
        }
    };
    // Availability is explicit; an unavailable query is never zero-cost work.
    inline double Microseconds(uint64_t begin, uint64_t end, bool beginReady,
                               bool endReady, uint32_t bits, double period)
    {
        return beginReady && endReady && bits ? double(TimestampTicks(begin, end, bits)) * period / 1000.0 : -1.0;
    }
}
