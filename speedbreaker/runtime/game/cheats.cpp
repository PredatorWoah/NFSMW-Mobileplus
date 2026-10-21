// SpeedBreaker One. GPL-3.0-or-later.
// Optional USA-specific career payout adjustment. No raw instruction patch,
// external trainer execution, save editing or arbitrary memory search.
#include <stdafx.h>
#include "cheats.h"
#include <kernel/function.h>
#include <kernel/vmem.h>
#include <user/paths.h>
#include <fstream>

extern "C" PPC_FUNC(__imp__sub_82337760);
namespace {
bool s_cash=false;
constexpr uint32_t kPayoutReturn=0x82364890;
bool ExpectedPayoutCode() {
    // Check the owning USA call and complete conversion/add/store sequence.
    constexpr uint32_t words[]={0x4BFD2ED5,0x38810050,0xFC00081E,0x815F000C,
        0x5783063E,0x2B030000,0x7C0027AE,0x81610050,0x7D6B5214,0x917F000C};
    auto* p=static_cast<const uint32_t*>(g_memory.Translate(kPayoutReturn-4));
    for(size_t i=0;i<std::size(words);i++)if(ByteSwap(p[i])!=words[i])return false;
    return true;
}
}
namespace game::cheats {
void Initialize(bool supportedUsa) {
    bool selected=false,blackEdition=false;
    std::ifstream in(GetUserPath()/"cheats.txt");
    for(std::string line;std::getline(in,line);) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line=="career_cash_payout=1")selected=true;
        if(line=="black_edition=1")blackEdition=true;
    }
    s_cash=selected && supportedUsa && ExpectedPayoutCode();
    // Xenia Canary game-patches, title 454107D9, "Black Edition content"
    // (tuhana). USA flag independently checked as zero in the original image.
    // Only data is written; the executable itself and disk dump stay untouched.
    if(blackEdition && supportedUsa) {
        auto* flag=static_cast<uint32_t*>(g_memory.Translate(0x82A2CE04));
        const uint32_t original=ByteSwap(*flag);
        uint32_t replacement;
        if(BlackEditionFlag(supportedUsa,original,replacement)) {
            *flag=ByteSwap(replacement);
            fprintf(stderr,"[cheats] Black Edition bonus content enabled; USA data flag 82A2CE04=00000100\n");
        } else fprintf(stderr,"[cheats] Black Edition NOT applied: unexpected original flag %08X\n",original);
    } else fprintf(stderr,"[cheats] Black Edition selected=%d eligible=%d active=0\n",blackEdition,supportedUsa);
    fprintf(stderr,"[cheats] career cash payout top-up selected=%d eligible=%d active=%d\n",
        selected,supportedUsa,s_cash);
}
}
PPC_FUNC(sub_82337760) {
    const uint32_t caller=uint32_t(ctx.lr), profile=ctx.r31.u32;
    __imp__sub_82337760(ctx,base);
    if(!s_cash || caller!=kPayoutReturn || (profile&3) || profile>UINT32_MAX-16)return;
    // r31 is the caller's live profile record (callee preserves it). Confirm
    // the allocator still owns this readable/writable word. No broad scan.
    const uint32_t address=profile+12;
    vmem::PageHeap* heap=vmem::HeapFor(address);
    vmem::RegionInfo info{};
    if(!heap || !heap->Query(address,&info) || info.state!=vmem::X_MEM_COMMIT ||
       !(info.protect&(0x04|0x40)) || uint64_t(address)+4>uint64_t(info.baseAddress)+info.regionSize)return;
    const uint32_t cash=PPC_LOAD_U32(address);
    const double old=ctx.f1.f64;
    ctx.f1.f64=game::cheats::CareerCashReward(cash,old);
    if(ctx.f1.f64!=old)fprintf(stderr,"[cheats] USA career payout: cash=%u reward=%.3f -> %.3f, target=%u; guest performs store\n",
        cash,old,ctx.f1.f64,game::cheats::kCareerCashTarget);
}
