// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The game's own dashboard art, from the player's default.xex: its XDBF
// resource (the one named after the title ID) holds the 64x64 title icon
// (image 0x8000) and the title's name (string 0x8000). Saved next to the
// settings as title-icon.png and title.txt for the Android launcher, which
// shows them and pins a home-screen shortcut with the icon. Nothing of the
// game is ever shipped in the app: these come from the player's own disc.
#pragma once
#include <cstdint>
#include <vector>

namespace title_art
{
    // After the XEX is loaded: `file` its bytes, `image`/`base` the loaded
    // image. Rewrites the files only when they differ. Logs what it saved.
    void Save(const std::vector<uint8_t>& file, const uint8_t* image, uint32_t base, uint32_t size);
}
