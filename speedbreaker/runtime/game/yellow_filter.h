// SpeedBreaker One. GPL-3.0-or-later.
//
// Settings > Display > Remove Game Yellow Filter. The yellow of Most Wanted is
// its own "Visual Look Effect" (Attrib::Gen::visuallookeffect, whose debug
// print the game ships: desaturation, blackbloom, colorbloom and colortint, a
// float triple). On the PC the defaults are colortint 0.88, 0.80, 0.44 (blue
// far under red and green is the yellow) and PC modders raise the blue to
// 0.95; this does the same to the live values in the game's memory.
//
// Narrow on purpose: it looks for exactly that triple of floats (or the
// colorbloom pair 0.9, 1.75 followed by a tint-shaped triple), only in
// committed read/write guest memory, keeps at most kMaxSpots addresses, writes
// one float each, and puts the original blue back when the setting goes off.
// Each find is logged with the floats around it ("[game-filter]").
#pragma once
#include <cstdint>
#include <cmath>

namespace game::visual_filter
{
    // A tint-shaped triple: red and green well above blue (the PC's 0.88, 0.80,
    // 0.44, the community's X360-like 0.68, 0.58, 0.03 and similar).
    inline bool TintShape(float r, float g, float b)
    {
        return std::isfinite(r) && std::isfinite(g) && std::isfinite(b) && r >= 0.5f && r <= 1.05f && g >= 0.45f && g <= 1.05f &&
            b >= 0.0f && b <= 0.65f && r - b >= 0.2f && g - b >= 0.15f && std::fabs(r - g) <= 0.3f;
    }

    // The blue that takes the yellow out: a little over the red and green mean
    // (0.88, 0.80 -> 0.90; the community's own fix is 0.95).
    inline float NeutralBlue(float r, float g)
    {
        const float mean = 0.5f * (r + g) + 0.06f;
        return mean > 1.0f ? 1.0f : mean;
    }

    // The game's render thread, each frame (VdSwap).
    void OnFrame();
}
