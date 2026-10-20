// clang++ -std=c++20 -O2 -Iruntime tests/android_schedule_test.cpp -o /tmp/schedule-test
#include <gpu/android_schedule.h>
#include <gpu/performance_window.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

int main()
{
    for (uint64_t f = 0; f < 10000; ++f)
    {
        assert(gpu::AndroidBarrierLevel(0, f) == 0);
        assert(gpu::AndroidBarrierLevel(2, f) == 2);
        assert(gpu::AndroidBarrierLevel(-1, f) == 0);
    }
    assert(gpu::AndroidBarrierLevel(4, 480) == 0);
    assert(gpu::AndroidBarrierLevel(4, 481) == 2);
    assert(gpu::AndroidBarrierLevel(4, 960) == 2);
    assert(gpu::AndroidBarrierLevel(4, 961) == 0);
    assert(gpu::TimestampTicks(250, 4, 8) == 10);
    assert(gpu::TimestampTicks(UINT64_MAX - 2, 3, 64) == 6);
    assert(gpu::TimestampTicks(100, 105, 0) == 0);
    gpu::PerformanceWindow w;
    assert(w.Percentile(.95) == 0);
    w.Add(-1); w.Add(0); w.Add(std::numeric_limits<double>::quiet_NaN());
    assert(w.count == 0);
    for (int i = 120; i; --i) w.Add(i);
    w.Add(999);
    assert(w.count == 120 && w.sum == 7260);
    assert(w.Percentile(.5) == 60);
    assert(w.Percentile(.95) == 114);
    assert(w.Percentile(.99) == 119);
    assert(w.Percentile(1) == 120);
}
