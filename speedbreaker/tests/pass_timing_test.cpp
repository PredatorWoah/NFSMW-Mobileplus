// g++ -std=c++20 -O2 -Iruntime tests/pass_timing_test.cpp -o /tmp/pass_timing_test
#include <gpu/pass_timing.h>
#include <cassert>
#include <cstdio>
int main()
{
    gpu::timing::Schedule schedule;
    for (int i = 0; i < 500; ++i) assert(!schedule.Next(1499));
    for (int i = 1; i <= 20000; ++i)
    {
        const bool expected = i % 120 == 30 && i <= 14310;
        assert(schedule.Next(1500) == expected);
    }
    assert(schedule.samples == 120);
    gpu::timing::Ledger ledger;
    for (unsigned i = 0; i < gpu::timing::MaxRegions; ++i) assert(ledger.Add() == int(i));
    assert(ledger.Add() == -1 && ledger.Add() == -1 && ledger.dropped == 2);
    ledger = {};
    assert(ledger.Add() == 0 && ledger.dropped == 0);
    using gpu::timing::Microseconds;
    assert(Microseconds(250, 10, true, true, 8, 1000) == 16);
    assert(Microseconds(UINT64_MAX - 5, 10, true, true, 64, 1000) == 16);
    assert(Microseconds(0, 0, true, true, 64, 1) == 0);
    assert(Microseconds(0, 10, false, true, 64, 1) == -1);
    assert(Microseconds(0, 10, true, false, 64, 1) == -1);
    assert(Microseconds(0, 10, true, true, 0, 1) == -1);
    puts("pass timing: bounded samples, query exhaustion, unavailable results and wrap passed");
}
