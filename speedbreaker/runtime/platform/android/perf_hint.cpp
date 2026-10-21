// SpeedBreaker One. GPL-3.0-or-later.
#include "../perf_hint.h"
#include "../../user/settings.h"
#include <dlfcn.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <type_traits>
#include <cstdio>
#include <mutex>
#include <vector>

namespace perfhint
{
    namespace
    {
        // The NDK's APerformanceHint (API 33; setThreads 34,
        // setPreferPowerEfficiency 35), looked up at run time.
        struct Manager;
        struct Session;
        struct Api
        {
            Manager* (*getManager)() = nullptr;
            Session* (*createSession)(Manager*, const int32_t*, size_t, int64_t) = nullptr;
            int (*updateTarget)(Session*, int64_t) = nullptr;
            int (*reportActual)(Session*, int64_t) = nullptr;
            void (*closeSession)(Session*) = nullptr;
            int (*setThreads)(Session*, const pid_t*, size_t) = nullptr;
            int (*setPreferPowerEfficiency)(Session*, bool) = nullptr;
        };

        const Api& Load()
        {
            static const Api api = [] {
                Api a;
                void* lib = dlopen("libandroid.so", RTLD_NOW);
                if (!lib)
                    return a;
                auto get = [&](auto& fn, const char* name) { fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(dlsym(lib, name)); };
                get(a.getManager, "APerformanceHint_getManager");
                get(a.createSession, "APerformanceHint_createSession");
                get(a.updateTarget, "APerformanceHint_updateTargetWorkDuration");
                get(a.reportActual, "APerformanceHint_reportActualWorkDuration");
                get(a.closeSession, "APerformanceHint_closeSession");
                get(a.setThreads, "APerformanceHint_setThreads");
                get(a.setPreferPowerEfficiency, "APerformanceHint_setPreferPowerEfficiency");
                return a;
            }();
            return api;
        }

        std::mutex s_lock;  // s_threads, s_threadsChanged
        std::vector<int32_t> s_threads;
        bool s_threadsChanged = false;

        // The command processor's alone (FrameDone).
        Session* s_session = nullptr;
        bool s_unavailable = false;  // no manager or session: don't try every frame
        int64_t s_target = 0, s_lastSwap = 0;
        double s_interval = 0;  // swap interval, smoothed (ns)

        int64_t Now()
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }

    void AddThisThread(const char* who)
    {
        std::lock_guard lock(s_lock);
        s_threads.push_back(int32_t(gettid()));
        s_threadsChanged = true;
        fprintf(stderr, "[adpf] %s (tid %d) in the performance hint session\n", who, int(gettid()));
    }

    void FrameDone(uint64_t busyNs)
    {
        const Api& api = Load();
        int64_t now = Now(), since = s_lastSwap ? now - s_lastSwap : 0;
        s_lastSwap = now;
        if (!settings::GetBool(settings::Id::PowerHints))
        {
            if (s_session)
            {
                api.closeSession(s_session);
                s_session = nullptr;
                fprintf(stderr, "[adpf] Power Hints off: session closed\n");
            }
            s_unavailable = false;  // on again: try again
            return;
        }
        // The frame period from the swaps (60 fps, or 30 with Frame Rate at
        // 30), past a pause; the target leaves a fifth of it for the waits on
        // the GPU and the display, so the hints never cost a frame.
        if (since > 0 && since < 200'000'000)
            s_interval = s_interval ? s_interval * 0.95 + double(since) * 0.05 : double(since);
        int64_t period = s_interval > 25e6 ? 33'333'333 : 16'666'667;
        int64_t target = period * 4 / 5;
        if (!s_session)
        {
            if (s_unavailable)
                return;
            std::vector<int32_t> threads;
            {
                std::lock_guard lock(s_lock);
                threads = s_threads;
                s_threadsChanged = false;
            }
            Manager* manager = api.getManager ? api.getManager() : nullptr;
            if (manager && api.createSession && api.reportActual && api.closeSession && !threads.empty())
                s_session = api.createSession(manager, threads.data(), threads.size(), target);
            if (!s_session)
            {
                s_unavailable = true;
                fprintf(stderr, "[adpf] no performance hint session on this device (manager %s)\n", manager ? "yes" : "no");
                return;
            }
            s_target = target;
            int efficient = api.setPreferPowerEfficiency ? api.setPreferPowerEfficiency(s_session, true) : -1;
            fprintf(stderr, "[adpf] performance hint session: %zu threads, target %.1f ms of a %.1f ms frame; prefer power efficiency: %s\n",
                threads.size(), double(target) / 1e6, double(period) / 1e6,
                efficient == 0 ? "yes" : api.setPreferPowerEfficiency ? "refused" : "no (before Android 15)");
        }
        if (target != s_target && api.updateTarget && api.updateTarget(s_session, target) == 0)
        {
            s_target = target;
            fprintf(stderr, "[adpf] target %.1f ms (%.1f ms frames)\n", double(target) / 1e6, double(period) / 1e6);
        }
        if (api.setThreads)
        {
            std::vector<int32_t> threads;
            {
                std::lock_guard lock(s_lock);
                if (s_threadsChanged)
                    threads = s_threads;
                s_threadsChanged = false;
            }
            if (!threads.empty())
                api.setThreads(s_session, threads.data(), threads.size());
        }
        if (busyNs > 0)
            api.reportActual(s_session, int64_t(std::min<uint64_t>(busyNs, 1'000'000'000)));
    }
}
