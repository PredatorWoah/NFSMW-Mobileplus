// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Android's performance hints (ADPF, APerformanceHint): the command processor
// and the game's main thread in one hint session, told each frame how long
// the command processor's work took against a target below the frame period.
// Without it the scheduler only sees busy threads and races them up to high
// clocks; with it the system can run them at the lowest clocks that still make
// the frame (and, Android 15 on, prefer power efficiency). On a vivo X300 Pro
// the game reached SEVERE heat in 2.5 min and CRITICAL in 6 at 60 fps.
//
// Settings > Advanced > Power Hints (on by default) turns it off, live, for
// comparing. Elsewhere, or where the system has no hint manager, nothing
// happens. The log says once what was set up ("[adpf] ...").
#pragma once
#include <cstdint>

namespace perfhint
{
#ifdef __ANDROID__
    // The calling thread joins the session (the command processor, nfsmw-main).
    void AddThisThread(const char* who);
    // Command processor, at each swap: this frame's busy time (ring execution
    // not blocked in a wait).
    void FrameDone(uint64_t busyNs);
#else
    inline void AddThisThread(const char*) {}
    inline void FrameDone(uint64_t) {}
#endif
}
