// SpeedBreaker. GPL-3.0-or-later.
// Adapted rule from codepdbh/nfsmw-android's nfsmw_nativo_dibujos.cpp.
#pragma once
#include <cstdint>
namespace gpu {
constexpr bool SafeEarlyDepth(uint32_t control, bool depthAttached, bool writesDepth,
                              bool memExport, bool occlusionOpen) {
    return depthAttached && (control & 2u) && !(control & 4u) && !(control & 1u) &&
           !writesDepth && !memExport && !occlusionOpen;
}
}
