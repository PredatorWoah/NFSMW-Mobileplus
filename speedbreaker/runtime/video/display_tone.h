// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The picture's tone curve in the presenter's last pass (video/presenter.cpp):
// Settings > Display > Brightness, and "Xbox 360 Display Gamma" (on by
// default: the ramp the game programs into the 360's display output,
// video/display_gamma.h). Both only touch the game's picture, never the menus
// drawn over it, and with Brightness 0 and the ramp off nothing is applied at
// all: the presenter draws with the same pipeline as before (Neutral()).
//
// Brightness is a power curve on the picture's values, as a console's
// brightness option (and the game's own) works: black stays black, white
// stays white, the shadows and mid-tones move. Each step multiplies the
// exponent by kStepRatio: mid-grey moves about 6 levels of 255 a step, and
// the ends (-5, +5) reach about as far as the games' own brightness range
// does from its default (their ramp's exponent runs 1.06 .. 0.57 around
// 0.815: x1.30 .. x0.70; here x1.40 .. x0.71).
//
// The 360's ramp, when on, comes first (it is the console's output, before
// the TV), then Brightness.
//
// The lookup table is 256 entries per channel, for framebuffer values k/255;
// the shader samples it with linear filtering at (v * 255 + 0.5) / 256, so a
// value between two entries (a scaled pixel) is interpolated.
//
// Header only, no runtime dependencies (tests/display_tone_test.cpp).
#pragma once
#include "display_gamma.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace video::tone
{
    constexpr int32_t kBrightnessMin = -5;
    constexpr int32_t kBrightnessMax = 5;
    constexpr int32_t kBrightnessDefault = 0;
    // The exponent's factor per step (brighter: smaller exponent).
    constexpr double kStepRatio = 0.935;

    // The power applied to the picture's values at a Brightness step
    // (clamped to the range): 1 at 0.
    inline double BrightnessExponent(int32_t step)
    {
        step = std::clamp(step, kBrightnessMin, kBrightnessMax);
        return step == 0 ? 1.0 : std::pow(kStepRatio, double(step));
    }

    // Brightness alone on one value in 0..1.
    inline double ApplyBrightness(double v, int32_t step)
    {
        v = std::clamp(v, 0.0, 1.0);
        double e = BrightnessExponent(step);
        return e == 1.0 ? v : std::pow(v, e);
    }

    // Yellow Tint Removal: the game's picture is graded yellow. Measured on two
    // phone screenshots (2026-10-23, 10-31): in daylight the road read red 133,
    // green 127, blue 94 (blue 1.4x under), the sky 254/255/232; in a safe house
    // red and green sat 1.4-1.6x over blue at levels 10-30. The cast changes
    // with the scene (the game's grade is per scene), so this is a correction
    // for the usual case, not an exact inverse. Blue gets a bump: v + c *
    // 4v(1-v), zero at black and white and largest in the mid-tones (+0.12 at
    // v = 0.5: the road's 94 becomes 122); red and green a small negative one
    // (-0.025: 133 and 127 become 127 and 121). Monotonic (slope at white
    // 1 - 4c: 0.52). Step n of 5 is n/5 of both bumps.
    constexpr int32_t kTintMin = 0;
    constexpr int32_t kTintMax = 5;
    constexpr double kTintBlueBump = 0.12;
    constexpr double kTintRedGreenBump = 0.025;

    // v after the bump for channel `c` (0 red, 1 green, 2 blue) at a step.
    inline double ApplyTint(double v, uint32_t c, int32_t step)
    {
        step = std::clamp(step, kTintMin, kTintMax);
        v = std::clamp(v, 0.0, 1.0);
        if (step == 0)
            return v;
        const double strength = double(step) / double(kTintMax);
        const double bump = 4.0 * v * (1.0 - v);
        const double amount = c == 2 ? kTintBlueBump : -kTintRedGreenBump;
        return std::clamp(v + amount * strength * bump, 0.0, 1.0);
    }

    // Nothing to apply: the presenter keeps its plain pipeline.
    inline bool Neutral(int32_t brightness, bool useRamp, int32_t tint = 0)
    {
        return std::clamp(brightness, kBrightnessMin, kBrightnessMax) == 0 && !useRamp && std::clamp(tint, kTintMin, kTintMax) == 0;
    }

    // out[k][c]: what framebuffer value k/255 of channel c (red, green, blue)
    // becomes, 0..1.
    struct Lut
    {
        std::array<std::array<float, 3>, 256> entry{};
    };

    // The table for a Brightness step and, when `ramp` is given, the 360's
    // display ramp before it.
    inline Lut BuildLut(int32_t brightness, const display_gamma::Table* ramp, int32_t tint = 0)
    {
        Lut lut;
        for (uint32_t k = 0; k < 256; k++)
            for (uint32_t c = 0; c < 3; c++)
            {
                double v = ramp ? ramp->entry[k][c] / 1023.0 : k / 255.0;
                v = ApplyBrightness(v, brightness);
                // After the console's ramp and Brightness: what is left is the
                // game's own grade, which this takes the yellow out of.
                v = ApplyTint(v, c, tint);
                lut.entry[k][c] = float(v);
            }
        return lut;
    }

    // IEEE half (the LUT image's R16G16B16A16_SFLOAT) from a float in 0..1,
    // rounded to nearest even. Values outside 0..1 are clamped.
    inline uint16_t ToHalf(float f)
    {
        f = std::clamp(f, 0.0f, 1.0f);
        if (f == 0.0f)
            return 0;
        int e = 0;
        float m = std::frexp(f, &e);  // f = m * 2^e, m in [0.5, 1)
        // Half: value = (1 + frac/1024) * 2^(exp - 15), exp 1..30 (normal),
        // or frac/1024 * 2^-14 (subnormal, exp 0).
        int exp = e - 1 + 15;
        if (exp <= 0)
        {
            // Subnormal: frac = f * 2^24.
            double frac = std::nearbyint(double(f) * 16777216.0);
            return uint16_t(frac);
        }
        double frac = (double(m) * 2.0 - 1.0) * 1024.0;
        double r = std::nearbyint(frac);  // ties to even (default rounding mode)
        if (r >= 1024.0)
        {
            r = 0.0;
            exp++;
        }
        return uint16_t((exp << 10) | int(r));
    }

    inline float FromHalf(uint16_t h)
    {
        int exp = (h >> 10) & 0x1F, frac = h & 0x3FF;
        if (exp == 0)
            return std::ldexp(float(frac), -24);
        return std::ldexp(1.0f + frac / 1024.0f, exp - 15);
    }

    // The LUT image's texels: 256 x RGBA16F (alpha 1).
    inline std::array<uint16_t, 256 * 4> ToHalfTexels(const Lut& lut)
    {
        std::array<uint16_t, 256 * 4> texels{};
        for (uint32_t k = 0; k < 256; k++)
        {
            for (uint32_t c = 0; c < 3; c++)
                texels[k * 4 + c] = ToHalf(lut.entry[k][c]);
            texels[k * 4 + 3] = ToHalf(1.0f);
        }
        return texels;
    }

    // What the shader does with the table (kToneGlsl in presenter.cpp), for
    // one channel value v in 0..1: linear between the two entries around
    // v * 255.
    inline double Sample(const Lut& lut, uint32_t channel, double v)
    {
        double x = std::clamp(v, 0.0, 1.0) * 255.0;
        uint32_t i = std::min(uint32_t(x), 254u);
        double t = x - i;
        return lut.entry[i][channel] * (1.0 - t) + lut.entry[i + 1][channel] * t;
    }
}
