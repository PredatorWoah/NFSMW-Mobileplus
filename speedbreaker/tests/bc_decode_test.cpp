// Game-free boundary checks of the exact integer decoder compiled into GLSL.
// clang++ -std=c++20 -O2 -Iruntime tests/bc_decode_test.cpp -o build/bc_decode_test
#include <gpu/bc_decode_glsl.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    using namespace gpu::bc;
    // RGB565 endpoint extremes and BC1's transparent fourth index.
    assert(sbBcPixel(5, 0x001FF800u, 0, 0, 0, 0) == 0xFF0000FFu);
    assert(sbBcPixel(5, 0x001FF800u, 0x55555555u, 0, 0, 0) == 0xFFFF0000u);
    assert(sbBcPixel(5, 0xFFFF0000u, 0xFFFFFFFFu, 0, 0, 0) == 0u);
    assert(sbBcPixel(5, 0x00000000u, 0xFFFFFFFFu, 0, 0, 0) == 0u);
    // BC2/3 always use four-color interpolation even if c0 <= c1.
    assert(sbBcPixel(6, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFF0000u, 0xFFFFFFFFu, 0) == 0xFFAAAAAAu);
    assert(sbBcPixel(7, 0x0000FFFFu, 0, 0xFFFF0000u, 0xFFFFFFFFu, 0) == 0xFFAAAAAAu);
    for (uint t = 0; t < 16; ++t) {
        // Every explicit alpha nibble, including the second word boundary.
        uint nibble = t & 15u, shift = (t & 7u) * 4u;
        uint lo = t < 8u ? nibble << shift : 0u;
        uint hi = t < 8u ? 0u : nibble << shift;
        assert((sbBcPixel(6, lo, hi, 0, 0, t) >> 24u) == nibble * 17u);
        // Every 3-bit index at every position: includes the cross-word index
        // starting at bit 31 and the final index starting at bit 61.
        for (uint index = 0; index < 8; ++index) {
            uint64_t bits = uint64_t(index) << (16 + t * 3);
            for (auto ends : {std::array<uint,2>{240,10}, {10,240}, {37,37}}) {
                uint a = ends[0], b = ends[1];
                uint w0 = uint(bits) | a | (b << 8), w1 = uint(bits >> 32);
                uint expected = index == 0 ? a : index == 1 ? b :
                    a > b ? ((8-index)*a+(index-1)*b)/7 :
                    index < 6 ? ((6-index)*a+(index-1)*b)/5 : index == 6 ? 0 : 255;
                assert(sbBcAlpha(w0,w1,t) == expected);
                uint pixel = sbBcPixel(8,w0,w1,w0,w1,t);
                assert(pixel == (expected | (expected << 8) | 0xFF000000u));
            }
        }
    }
    // All 65,536 RGB565 endpoints: check expansion against rational endpoints
    // (nearest integer) with the permitted <=1 LSB replication difference.
    for (uint value = 0; value < 65536u; ++value) {
        for (uint c = 0; c < 3; ++c) {
            uint v = c == 0 ? value >> 11 : c == 1 ? (value >> 5) & 63 : value & 31;
            uint max = c == 1 ? 63 : 31, ideal = (v * 255 + max/2)/max;
            uint actual = sbBc565(value,c);
            assert(actual <= 255 && actual + 1 >= ideal && ideal + 1 >= actual);
        }
    }
    constexpr uint64_t slot = 16ull << 20;
    assert(BlockRowsPerChunk(0,1,slot) == 0);
    assert(BlockRowsPerChunk(1,0,slot) == 0);
    assert(BlockRowsPerChunk(1,1,63) == 0);
    assert(BlockRowsPerChunk(1,1,64) == 1);
    assert(BlockRowsPerChunk(1024,1024,slot) == 256); // 4096x4096: four safe chunks.
    assert(BlockRowsPerChunk(2048,2048,slot) == 128); // 8192x8192: sixteen chunks.
    for (uint width : {1u,3u,4u,5u,17u,1023u,1024u,4095u,8192u}) {
        for (uint height : {1u,3u,4u,5u,13u,1024u,4097u,8192u}) {
            uint bw = (width + 3)/4, bh = (height + 3)/4;
            uint rows = BlockRowsPerChunk(bw,bh,slot), copied = 0;
            assert(rows);
            for (uint first=0; first<bh; first+=rows) {
                uint count = rows < bh-first ? rows : bh-first;
                assert(uint64_t(bw)*64*count <= slot);
                uint remain = height-first*4, chunk = count*4;
                copied += remain < chunk ? remain : chunk;
            }
            assert(copied == height);
        }
    }
    if (argc == 2) {
        // Compile this exact shader on the host/device, without any game data.
        std::ofstream shader(argv[1]);
        shader << "#version 450\nlayout(local_size_x=64) in;\n"
               << "layout(std430,set=0,binding=0) buffer B { uint data[]; };\n"
               << "layout(push_constant) uniform P { uint blocks; } p;\n"
               << GLSL_BC_DECODE
               << "void main() { uint t=gl_GlobalInvocationID.x; if(t>=p.blocks*16u) return; uint b=(t/16u)*5u; "
               << "data[p.blocks*5u+t]=sbBcPixel(data[b],data[b+1u],data[b+2u],data[b+3u],data[b+4u],t&15u); }\n";
        assert(shader.good());
    }
    std::puts("BC endpoint, transparency, interpolation, alpha index boundaries: PASS");
}
