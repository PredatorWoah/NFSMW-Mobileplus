// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See thermal.h.
#include "thermal.h"

#include <report/report.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#ifdef __ANDROID__
#include <dlfcn.h>
#endif

namespace platform::thermal
{
    namespace
    {
        std::atomic<State> s_state{ State::Unknown };

        struct Change
        {
            double at;  // seconds since launch (report::SessionSeconds)
            State state;
        };

        bool FromName(std::string_view name, State& state)
        {
            for (State s : { State::Nominal, State::Fair, State::Serious, State::Critical })
                if (name == Name(s))
                {
                    state = s;
                    return true;
                }
            return false;
        }

        // NFSMW_THERMAL_SIM="<seconds>:<state>,...", in time order. A bad
        // entry is logged and skipped.
        const std::vector<Change>& Simulated()
        {
            static const std::vector<Change> changes = [] {
                std::vector<Change> parsed;
                const char* v = std::getenv("NFSMW_THERMAL_SIM");
                if (!v)
                    return parsed;
                for (std::string_view rest = v; !rest.empty();)
                {
                    size_t comma = rest.find(',');
                    std::string item(rest.substr(0, comma));
                    rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
                    size_t colon = item.find(':');
                    char* end = nullptr;
                    double at = colon == std::string::npos ? -1.0 : std::strtod(item.c_str(), &end);
                    State state;
                    if (colon == std::string::npos || end != item.c_str() + colon || !(at >= 0.0) ||
                        !FromName(std::string_view(item).substr(colon + 1), state))
                    {
                        fprintf(stderr, "[thermal] NFSMW_THERMAL_SIM: \"%s\" isn't <seconds>:<nominal|fair|serious|critical>; skipped\n",
                            item.c_str());
                        continue;
                    }
                    parsed.push_back({ at, state });
                }
                std::stable_sort(parsed.begin(), parsed.end(), [](const Change& a, const Change& b) { return a.at < b.at; });
                std::string text;
                for (const Change& c : parsed)
                    text += std::format("{}{} at {} s", text.empty() ? "" : ", ", Name(c.state), c.at);
                if (!parsed.empty())
                    fprintf(stderr, "[thermal] NFSMW_THERMAL_SIM: %s (instead of the system's)\n", text.c_str());
                return parsed;
            }();
            return changes;
        }
    }

#ifdef __ANDROID__
    namespace
    {
        // Android's thermal status (AThermal, API 30, looked up at run time):
        // none/light -> nominal, moderate -> fair, severe -> serious (Frame
        // Rate Auto's "hot": the vivo X300 Pro throttles its clocks there),
        // critical and up -> critical.
        State ReadAndroid()
        {
            struct Api
            {
                void* (*acquire)() = nullptr;
                int (*status)(void*) = nullptr;
                void* manager = nullptr;
            };
            static Api api = [] {
                Api a;
                if (void* lib = dlopen("libandroid.so", RTLD_NOW))
                {
                    a.acquire = reinterpret_cast<void* (*)()>(dlsym(lib, "AThermal_acquireManager"));
                    a.status = reinterpret_cast<int (*)(void*)>(dlsym(lib, "AThermal_getCurrentThermalStatus"));
                    if (a.acquire && a.status)
                        a.manager = a.acquire();
                }
                return a;
            }();
            if (!api.manager)
                return State::Unknown;
            const int status = api.status(api.manager);
            return status < 0 ? State::Unknown : status <= 1 ? State::Nominal : status == 2 ? State::Fair
                : status == 3 ? State::Serious : State::Critical;
        }
    }
#endif

    State Current()
    {
        return s_state.load(std::memory_order_relaxed);
    }

    const char* Name(State state)
    {
        switch (state)
        {
        case State::Nominal: return "nominal";
        case State::Fair: return "fair";
        case State::Serious: return "serious";
        case State::Critical: return "critical";
        default: return "unknown";
        }
    }

    void Poll()
    {
        // About once a second: the state changes over tens of seconds, and
        // Auto's timers are 30-60 s.
        static double next = 0.0;
        static bool first = true;
        const double now = report::SessionSeconds();
        if (now < next)
            return;
        next = now + 1.0;

        State state = State::Unknown;
#ifdef __APPLE__
        state = detail::ReadSystem();
#elif defined(__ANDROID__)
        state = ReadAndroid();
#endif
        bool simulated = false;
        for (const Change& c : Simulated())
        {
            if (c.at > now)
                break;
            state = c.state;
            simulated = true;
        }

        State old = s_state.exchange(state, std::memory_order_relaxed);
        if (std::exchange(first, false))
        {
            if (state != State::Unknown)
                fprintf(stderr, "[thermal] %s at start%s\n", Name(state), simulated ? " (NFSMW_THERMAL_SIM)" : "");
        }
        else if (state != old)
            fprintf(stderr, "[thermal] %s -> %s at %.1f s%s\n", Name(old), Name(state), now, simulated ? " (NFSMW_THERMAL_SIM)" : "");
    }
}
