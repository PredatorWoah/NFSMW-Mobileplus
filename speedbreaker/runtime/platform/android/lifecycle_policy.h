// SpeedBreaker. GPL-3.0-or-later.
#pragma once
#include <atomic>
#include <cstdint>
namespace platform::android {
// Focus and Activity lifecycle are independent. A focus event must not resume
// a still-background Activity; duplicate callbacks must be harmless.
class LifecyclePolicy {
    std::atomic<uint32_t> reasons{0};
    bool Set(uint32_t bit, bool inactive) {
        const uint32_t previous = inactive ? reasons.fetch_or(bit) : reasons.fetch_and(~bit);
        return (inactive ? previous | bit : previous & ~bit) == 0;
    }
public:
    bool Focus(bool focused) { return Set(1u, !focused); }
    bool Background(bool background) { return Set(2u, background); }
    bool Active() const { return reasons.load() == 0; }
};
}
