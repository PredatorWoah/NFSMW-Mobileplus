// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <atomic>
#include <cstdint>
#ifdef __ANDROID__
#include <cassert>
#include <climits>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace platform {
// NDK r28's libc++ has no atomic_ref. All runtime uses reference aligned
// uint32_t guest words. Keep seq_cst semantics and wait/notify without
// reinterpreting ordinary guest storage as a constructed std::atomic.
class AtomicRef {
    uint32_t* value;
public:
    explicit AtomicRef(uint32_t& ref) : value(&ref) { assert(reinterpret_cast<uintptr_t>(value) % alignof(uint32_t) == 0); }
    uint32_t load() const { return __atomic_load_n(value, __ATOMIC_SEQ_CST); }
    void store(uint32_t v) const { __atomic_store_n(value, v, __ATOMIC_SEQ_CST); }
    uint32_t operator=(uint32_t v) const { store(v); return v; }
    bool compare_exchange_weak(uint32_t& expected, uint32_t desired) const {
        return __atomic_compare_exchange_n(value, &expected, desired, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
    bool compare_exchange_strong(uint32_t& expected, uint32_t desired) const {
        return __atomic_compare_exchange_n(value, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
    void wait(uint32_t old) const {
        // Shared futex keys preserve physical aliases. EAGAIN, EINTR and
        // spurious wakes all recheck the value, as atomic_ref::wait does.
        while (load() == old) syscall(SYS_futex, value, FUTEX_WAIT, old, nullptr, nullptr, 0);
    }
    void notify_one() const { syscall(SYS_futex, value, FUTEX_WAKE, 1, nullptr, nullptr, 0); }
};
}
#else
namespace platform { template<class T> using AtomicRef = std::atomic_ref<T>; }
#endif
