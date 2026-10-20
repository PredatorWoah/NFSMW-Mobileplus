// SpeedBreaker runtime. GPL-3.0-or-later.
#pragma once
#include <cstdint>
namespace gpu {
// No fragment side effects may be lost. Geometry and native depth/stencil stay.
constexpr bool CanOmitFragmentStage(uint32_t colorMask, bool alphaTest, bool kill,
    bool depthExport, bool pixelMemExport, bool vertexMemExport) {
    return colorMask == 0 && !alphaTest && !kill && !depthExport && !pixelMemExport && !vertexMemExport;
}
}
