// SpeedBreaker runtime. GPL-3.0-or-later. Synthetic instruction streams only.
#include <gpu/shader_translator.h>
#include <gpu/xenos/ucode.h>
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using Op = std::array<uint32_t,3>;
using namespace xe::gpu::ucode;
static Op Alu(int predicate, unsigned scalar=50, unsigned vector=2) {
    return { (15u<<16) | (1u<<15) | (scalar<<26),
        predicate<0 ? 0u : (1u<<28) | (uint32_t(predicate)<<27),
        (vector<<24) | (1u<<31) | (1u<<30) };
}
static Op Texture(int predicate) {
    // Explicit LOD fetch with half-texel offset, matching shader_variants_test.
    return {1u | (1u<<20), 0x688u | (predicate<0 ? 0u : (1u<<31)), (1u<<16) | (1u<<14) |
        (predicate<0 ? 0u : (uint32_t(predicate)<<31))};
}
static std::vector<uint32_t> Program(std::vector<Op> ops, uint32_t fetch=0, bool split=false, unsigned stateMachine=0, bool outer=false) {
    std::vector<ControlFlowInstruction> cf(4);
    unsigned first = stateMachine ? 2 : 1;
    cf[0].dword_0 = first | ((split ? 1u : uint32_t(ops.size()))<<12) | (fetch<<16);
    cf[0].dword_1 = uint32_t(outer ? ControlFlowOpcode::kCondExecPred : (split ? ControlFlowOpcode::kExec : ControlFlowOpcode::kExecEnd))<<12;
    if (outer) cf[0].dword_1 |= 1u<<10;
    cf[1].dword_0 = split ? first+1 | (uint32_t(ops.size()-1)<<12) | ((fetch>>2)<<16) : first;
    cf[1].dword_1 = (split || outer) ? uint32_t(ControlFlowOpcode::kExecEnd)<<12 : 0;
    if (stateMachine) {
        // A conditional jump after the first EXEC forces the state-machine path.
        cf[2] = cf[1];
        cf[1].dword_0 = 2;
        cf[1].dword_1 = uint32_t(ControlFlowOpcode::kCondJmp)<<12;
    }
    if (stateMachine == 2) {
        // A loop around EXEC, then a separate EXEC after the loop.
        cf[3]=cf[2];
        cf[1]=cf[0];
        cf[0].dword_0=3; cf[0].dword_1=uint32_t(ControlFlowOpcode::kLoopStart)<<12;
        cf[2].dword_0=1; cf[2].dword_1=uint32_t(ControlFlowOpcode::kLoopEnd)<<12;
    } else if (stateMachine == 3) {
        // The called block contains the second EXEC; return ends the subroutine.
        cf[3].dword_0=0; cf[3].dword_1=uint32_t(ControlFlowOpcode::kReturn)<<12;
        cf[1].dword_0=2 | (1u<<13); cf[1].dword_1=uint32_t(ControlFlowOpcode::kCondCall)<<12;
        cf[2].dword_1=uint32_t(ControlFlowOpcode::kExec)<<12;
    }
    std::vector<uint32_t> words;
    for (unsigned i=0;i<first;++i) {
        auto& a=cf[i*2]; auto& b=cf[i*2+1];
        words.insert(words.end(),{a.dword_0,a.dword_1|(b.dword_0<<16),(b.dword_0>>16)|(b.dword_1<<16)});
    }
    for (auto op:ops) words.insert(words.end(),op.begin(),op.end());
    return words;
}
static unsigned Count(const std::string& text,const std::string& needle) {
    unsigned n=0; size_t pos=0; while ((pos=text.find(needle,pos))!=std::string::npos) { ++n; pos+=needle.size(); } return n;
}
int main(int argc,char** argv) {
    struct Case { const char* name; std::vector<Op> ops; unsigned fetch, branches; bool split=false; unsigned state=0; bool outer=false; };
    std::vector<Case> cases{
        {"same",{Alu(1),Alu(1),Alu(1)},0,1},
        {"opposite",{Alu(1),Alu(0),Alu(1)},0,3},
        {"unpredicated",{Alu(1),Alu(-1),Alu(1)},0,2},
        {"scalar-write",{Alu(1),Alu(1,27),Alu(1)},0,2},
        {"vector-write",{Alu(1),Alu(1,50,20),Alu(1)},0,2},
        {"paired-write",{Alu(1),Alu(1,27,20),Alu(1)},0,2},
        {"exec-boundary",{Alu(1),Alu(1),Alu(1)},0,2,true},
        {"state-machine",{Alu(1),Alu(1),Alu(1)},0,2,true,true},
        {"loop-boundary",{Alu(1),Alu(1),Alu(1)},0,2,true,2},
        {"call-boundary",{Alu(1),Alu(1),Alu(1)},0,2,true,3},
        {"outer-write",{Alu(1),Alu(1,27),Alu(1)},0,3,false,false,true},
        {"mixed-fetch",{Texture(1),Alu(1),Texture(1)},17,1},
    };
    if (argc==2) std::filesystem::create_directories(argv[1]);
    for (auto& c:cases) {
        auto words=Program(c.ops,c.fetch,c.split,c.state,c.outer);
        // Verify actual encoding, rather than accidentally testing unpredicated ops.
        if (c.fetch) { auto f=reinterpret_cast<const TextureFetchInstruction*>(c.ops[0].data()); assert(f->is_predicated() && f->predicate_condition()); }
        gpu::g_predicateCoalescingEnabled=true;
        auto merged=gpu::TranslateShader(gpu::ShaderStage::Pixel,words.data(),words.size());
        gpu::g_predicateCoalescingEnabled=false;
        auto old=gpu::TranslateShader(gpu::ShaderStage::Pixel,words.data(),words.size());
        assert(merged.ok && old.ok);
        auto branches=Count(merged.glsl,"if (p0 == ");
        if (branches!=c.branches) { std::cerr << c.name << ": " << branches << " expected " << c.branches << '\n'; return 1; }
        assert(Count(old.glsl,"if (p0 == ")>=branches);
        assert(merged.predicateBlocks == branches - unsigned(c.outer));
        assert(merged.predicatedInstructions == old.predicatedInstructions);
        assert(Count(merged.glsl,"vec4 tdata = ")==Count(old.glsl,"vec4 tdata = "));
        assert(merged.texture2DMask==old.texture2DMask && merged.usesKill==old.usesKill);
        if (argc==2) for (bool optimized:{false,true}) {
            auto source=optimized?merged.glsl:old.glsl;
            source.insert(source.find('\n')+1,"#define OUT0\n");
            std::ofstream(std::filesystem::path(argv[1])/(std::string(c.name)+(optimized?"-merged.frag":"-old.frag")))<<source;
        }
    }
    gpu::g_predicateCoalescingEnabled=true;
    std::cout << "Predicate generations, CF boundaries and mixed fetch tests passed\n";
}
