// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace gpu
{
    // Intervals between CP swap callbacks, not Android display timestamps.
    // No allocation, GPU readback or synchronization in the sample path.
    struct PerformanceWindow
    {
        std::array<double, 120> samples{};
        size_t count = 0;
        double sum = 0;
        void Add(double ms)
        {
            if (!std::isfinite(ms) || ms <= 0 || count == samples.size()) return;
            samples[count++] = ms;
            sum += ms;
        }
        double Percentile(double fraction) const
        {
            if (!count) return 0;
            auto sorted = samples;
            std::sort(sorted.begin(), sorted.begin() + count);
            const auto rank = size_t(std::ceil(std::clamp(fraction, 0.0, 1.0) * double(count)));
            return sorted[rank ? rank - 1 : 0];
        }
    };
}
