// SpeedBreaker One. GPL-3.0-or-later.
// Host semantic test for the Android compiler-atomic/futex adapter:
// g++ -std=c++20 -D__ANDROID__ -pthread -Iruntime tests/android_atomic_ref_test.cpp -o build/android_atomic_ref_test
#include <platform/atomic_ref.h>
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
int main() {
    alarm(10); // A missed futex wake must fail, not hang the test runner.
    alignas(4) uint32_t lock = 0;
    uint32_t counter = 0;
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; i++) workers.emplace_back([&] {
        platform::AtomicRef ref(lock);
        for (int j = 0; j < 10000; j++) {
            uint32_t expected = 0;
            while (!ref.compare_exchange_weak(expected, 1)) { expected = 0; std::this_thread::yield(); }
            counter++;
            ref.store(0);
        }
    });
    for (auto& t : workers) t.join();
    assert(counter == 40000);
    int fd = int(syscall(SYS_memfd_create, "atomic-ref-alias-test", 1));
    assert(fd >= 0 && ftruncate(fd, 4096) == 0);
    auto* a = static_cast<uint32_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    auto* c = static_cast<uint32_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    assert(a != MAP_FAILED && c != MAP_FAILED);
    platform::AtomicRef ar(*a), cr(*c); ar.store(7);
    std::atomic<bool> ready{false}, resumed{false};
    std::thread waiter([&] { ready.store(true); ar.wait(7); resumed.store(ar.load() == 9); });
    while (!ready.load()) std::this_thread::yield();
    // Give the waiter a chance to enter FUTEX_WAIT on the other virtual alias.
    usleep(20000); cr.store(9); cr.notify_one(); waiter.join(); assert(resumed.load());
    uint32_t wrong = 4; assert(!ar.compare_exchange_strong(wrong, 10) && wrong == 9);
    munmap(a, 4096); munmap(c, 4096); close(fd); alarm(0);
}
