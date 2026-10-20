// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
// BC1/2/3/5 decode for the existing GPU untile boundary, without shaderInt64.
// Endpoint/index semantics cross-checked against NFSMW-Mobileplus's
// nativo/app/src/nfsmw_texturas_bc.h (GPL-3.0). Guest endian conversion occurs
// before these functions. The shader and host tests evaluate the same tokens.
#pragma once
#include <cstdint>

#define SB_BC_ALPHA_BODY \
    uint a = lo & 255u, b = (lo >> 8u) & 255u; \
    uint shift = 16u + 3u * texel; \
    uint index = shift < 32u ? lo >> shift : hi >> (shift - 32u); \
    if (shift == 31u) index |= hi << 1u; \
    index &= 7u; \
    if (index == 0u) return a; \
    if (index == 1u) return b; \
    if (a > b) return ((8u - index) * a + (index - 1u) * b) / 7u; \
    if (index < 6u) return ((6u - index) * a + (index - 1u) * b) / 5u; \
    return index == 6u ? 0u : 255u;

#define SB_BC_565_BODY \
    if (channel == 1u) { uint v = (value >> 5u) & 63u; return (v << 2u) | (v >> 4u); } \
    uint v = channel == 0u ? value >> 11u : value & 31u; \
    return (v << 3u) | (v >> 2u);

#define SB_BC_COLOR_BODY \
    uint a = endpoints & 65535u, b = endpoints >> 16u; \
    uint index = (indices >> (2u * texel)) & 3u; \
    uint ca = sbBc565(a, channel), cb = sbBc565(b, channel); \
    if (index == 0u) return ca; \
    if (index == 1u) return cb; \
    if (opaque || a > b) return index == 2u ? (2u * ca + cb) / 3u : (ca + 2u * cb) / 3u; \
    return index == 2u ? (ca + cb) / 2u : 0u;

// kind is the untile conversion: 5 BC1, 6 BC2, 7 BC3, 8 BC5.
// BC5's unused channels match the sampled BC5 format: B=0, A=1.
#define SB_BC_PIXEL_BODY \
    if (kind == 8u) return sbBcAlpha(w0, w1, texel) | (sbBcAlpha(w2, w3, texel) << 8u) | 0xFF000000u; \
    uint endpoints = kind == 5u ? w0 : w2, indices = kind == 5u ? w1 : w3; \
    uint alpha = 255u; \
    if (kind == 5u && (endpoints & 65535u) <= (endpoints >> 16u) && ((indices >> (2u * texel)) & 3u) == 3u) alpha = 0u; \
    if (kind == 6u) alpha = (((texel < 8u ? w0 : w1) >> (4u * (texel & 7u))) & 15u) * 17u; \
    if (kind == 7u) alpha = sbBcAlpha(w0, w1, texel); \
    return sbBcColor(endpoints, indices, texel, 0u, kind != 5u) | \
        (sbBcColor(endpoints, indices, texel, 1u, kind != 5u) << 8u) | \
        (sbBcColor(endpoints, indices, texel, 2u, kind != 5u) << 16u) | (alpha << 24u);

#define SB_BC_STRINGIFY_(...) #__VA_ARGS__
#define SB_BC_STRINGIFY(...) SB_BC_STRINGIFY_(__VA_ARGS__)
#define GLSL_BC_DECODE "\n" \
    "uint sbBcAlpha(uint lo, uint hi, uint texel) { " SB_BC_STRINGIFY(SB_BC_ALPHA_BODY) " }\n" \
    "uint sbBc565(uint value, uint channel) { " SB_BC_STRINGIFY(SB_BC_565_BODY) " }\n" \
    "uint sbBcColor(uint endpoints, uint indices, uint texel, uint channel, bool opaque) { " SB_BC_STRINGIFY(SB_BC_COLOR_BODY) " }\n" \
    "uint sbBcPixel(uint kind, uint w0, uint w1, uint w2, uint w3, uint texel) { " SB_BC_STRINGIFY(SB_BC_PIXEL_BODY) " }\n"

namespace gpu::bc {
using uint = uint32_t;
inline uint sbBcAlpha(uint lo, uint hi, uint texel) { SB_BC_ALPHA_BODY }
inline uint sbBc565(uint value, uint channel) { SB_BC_565_BODY }
inline uint sbBcColor(uint endpoints, uint indices, uint texel, uint channel, bool opaque) { SB_BC_COLOR_BODY }
inline uint sbBcPixel(uint kind, uint w0, uint w1, uint w2, uint w3, uint texel) { SB_BC_PIXEL_BODY }
// Four rows of four RGBA8 texels per block. Keep the upload ring unchanged.
inline uint BlockRowsPerChunk(uint blocksWide, uint blocksHigh, uint64_t slotBytes) {
    if (!blocksWide || !blocksHigh) return 0;
    const uint64_t rows = slotBytes / (uint64_t(blocksWide) * 64);
    return rows < blocksHigh ? uint(rows) : blocksHigh;
}
}
