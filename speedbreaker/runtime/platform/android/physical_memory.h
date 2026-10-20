// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>
namespace platform::android {
// The caller owns a contiguous, aligned 4 GiB guest reservation.
bool MapPhysicalMemory(uint8_t* base, uint32_t& eWindowShift);
}
