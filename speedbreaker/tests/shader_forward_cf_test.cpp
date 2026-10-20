// Synthetic CF programs exercise the actual translator, without game data.
#include <gpu/shader_translator.h>
#include <gpu/xenos/ucode.h>
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace xe::gpu::ucode;
static std::vector<uint32_t> Program(unsigned target, unsigned kind, ControlFlowOpcode end) {
    std::array<ControlFlowInstruction,6> cf{};
    cf[0].dword_0=3u|(1u<<12);cf[0].dword_1=uint32_t(ControlFlowOpcode::kExec)<<12;
    cf[1].dword_0=target|(kind==0 ? 1u<<13 : kind==1 ? 1u<<14 : 0);
    cf[1].dword_1=(uint32_t(ControlFlowOpcode::kCondJmp)<<12)|(1u<<10)|(7u<<2);
    cf[2]=cf[0];cf[3]=cf[1];cf[3].dword_0=5u|(1u<<13);
    cf[4]=cf[0];cf[4].dword_1=uint32_t(end)<<12;
    cf[5]=cf[0];cf[5].dword_1=uint32_t(ControlFlowOpcode::kExecEnd)<<12;
    std::vector<uint32_t> words;
    for(unsigned i=0;i<6;i+=2) words.insert(words.end(),{cf[i].dword_0,cf[i].dword_1|(cf[i+1].dword_0<<16),(cf[i+1].dword_0>>16)|(cf[i+1].dword_1<<16)});
    // MAX color export, scalar retain-prev. Reused by each EXEC.
    words.insert(words.end(),{(1u<<15)|(15u<<16)|(50u<<26),0,(2u<<24)|(1u<<31)|(1u<<30)});
    return words;
}
static unsigned Count(const std::string& s,const std::string& token) {
    unsigned n=0;size_t at=0;while((at=s.find(token,at))!=std::string::npos){++n;at+=token.size();}return n;
}
int main(int argc,char** argv) {
    if(argc==2)std::filesystem::create_directories(argv[1]);
    unsigned cases=0;
    for(unsigned target:{0u,1u,2u,4u,5u,6u,8191u})for(unsigned kind=0;kind<3;++kind)
        for(auto end:{ControlFlowOpcode::kExecEnd,ControlFlowOpcode::kCondExecEnd,ControlFlowOpcode::kCondExecPredEnd}) {
            auto words=Program(target,kind,end);
            gpu::g_forwardControlFlowEnabled=false;
            auto old=gpu::TranslateShader(gpu::ShaderStage::Pixel,words.data(),words.size());
            gpu::g_forwardControlFlowEnabled=true;
            auto revised=gpu::TranslateShader(gpu::ShaderStage::Pixel,words.data(),words.size());
            assert(old.ok && revised.ok);
            if(target<=1) assert(revised.glsl==old.glsl); // self/back edges must retain dispatcher
            else {
                assert(revised.glsl.find("switch (cfpc)")==std::string::npos);
                assert(revised.glsl.find("while (cfpc >= 0)")==std::string::npos);
                assert(Count(revised.glsl,"} while (false); }")==6);
                assert(Count(revised.glsl,"vec4 vres =")==Count(old.glsl,"vec4 vres ="));
                assert(revised.colorTargetMask==old.colorTargetMask && revised.interpolatorMask==old.interpolatorMask);
                // END remains outside conditional EXEC guards, matching the old
                // dispatcher. The surrounding do scope consumes jump breaks.
                assert(revised.glsl.find("cfpc = -1; break;")!=std::string::npos);
            }
            if(argc==2 && target==4 && end==ControlFlowOpcode::kExecEnd) {
                auto source=revised.glsl;source.insert(source.find('\n')+1,"#define OUT0\n");
                std::ofstream(std::filesystem::path(argv[1])/("forward-"+std::to_string(kind)+".frag"))<<source;
            }
            ++cases;
        }
    // Existing predicate suite covers loop/call boundaries. Require identical
    // generated source for those paths with the forward flag enabled as well.
    for(auto opcode:{ControlFlowOpcode::kLoopStart,ControlFlowOpcode::kLoopEnd,ControlFlowOpcode::kCondCall,ControlFlowOpcode::kReturn}) {
        auto words=Program(4,0,ControlFlowOpcode::kExecEnd);
        words[2]=(words[2]&0x0FFFFFFFu)|(uint32_t(opcode)<<28); // replace CF1 opcode
        gpu::g_forwardControlFlowEnabled=false;
        auto old=gpu::TranslateShader(gpu::ShaderStage::Pixel,words.data(),words.size());
        gpu::g_forwardControlFlowEnabled=true;
        auto revised=gpu::TranslateShader(gpu::ShaderStage::Pixel,words.data(),words.size());
        assert(old.ok && revised.ok && old.glsl==revised.glsl);
        ++cases;
    }
    gpu::g_forwardControlFlowEnabled=false;
    std::cout<<cases<<" forward/self/back/out-of-range/END/loop/call CF checks passed\n";
}
