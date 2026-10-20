// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Cheats chosen in the Android CHEATS menu (or by hand), applied as guest
// data writes right after the XEX image is copied into guest memory and
// before its entry point runs: where Xenia applies its game patches.
//
// Only data can be patched this way. The game's code was recompiled ahead of
// time, so writing PowerPC instructions into guest memory changes nothing;
// a cheat that needs different behaviour needs a hook (config/nfsmw.toml).
//
// The choices are read from <user folder>/cheats.txt, one "id=1" per line
// (the launcher writes it). Every patch checks the value it replaces first
// and is skipped, with a log line, when that value is not one it expects:
// on another executable, a wrong address must not become a corrupted game.
#pragma once

namespace cheats
{
    // Reads cheats.txt and applies the enabled data patches. Call once, after
    // the image is in guest memory and before the guest's entry point.
    void ApplyStartup();
}
