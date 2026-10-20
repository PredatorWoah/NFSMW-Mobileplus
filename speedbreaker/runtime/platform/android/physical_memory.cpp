// SpeedBreaker One. GPL-3.0-or-later.
#include "physical_memory.h"
#include <android/sharedmem.h>
#include <cerrno>
#include <cstdio>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace platform::android {
bool MapPhysicalMemory(uint8_t* base, uint32_t& eWindowShift) {
    constexpr size_t size = 0x20000000;
    const long page = sysconf(_SC_PAGESIZE);
    // This initial target preserves the hardware's exact +4 KiB E alias.
    // Do not silently substitute the unshifted Apple mapping on Android.
    if (page != 4096) {
        fprintf(stderr, "[memory] Android bring-up requires 4096-byte pages; got %ld\n", page);
        return false;
    }
    int fd = -1;
#ifdef SYS_memfd_create
    fd = int(syscall(SYS_memfd_create, "SpeedBreaker-physical", 1U /* MFD_CLOEXEC */));
    if (fd >= 0 && ftruncate(fd, size) != 0) { close(fd); fd = -1; }
#endif
    if (fd < 0) fd = ASharedMemory_create("SpeedBreaker-physical", size);
    if (fd < 0) { perror("[memory] shared backing"); return false; }
    struct View { uint32_t guest; size_t offset, length; };
    const View views[] = {{0xA0000000, 0, size}, {0xC0000000, 0, size}, {0xE0000000, 4096, size - 4096}};
    bool ok = true;
    for (const auto& v : views) {
        void* at = base + v.guest;
        if (mmap(at, v.length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, v.offset) == MAP_FAILED) {
            perror("[memory] physical alias mmap"); ok = false; break;
        }
        fprintf(stderr, "[memory] guest=%08x host=%p backing-offset=%zu length=%zu\n", v.guest, at, v.offset, v.length);
    }
    close(fd);
    if (ok) eWindowShift = 4096;
    return ok;
}
}
