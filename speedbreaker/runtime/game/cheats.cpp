// SpeedBreaker One. GPL-3.0-or-later.
// Optional USA-specific cheats: a career payout adjustment and the game's own
// unlimited nitro / SpeedBreaker flags. No raw instruction patch, external
// trainer execution, save editing or arbitrary memory search.
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
uint32_t Word(uint32_t address) {return ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(address)));}
uint8_t Byte(uint32_t address) {return *static_cast<const uint8_t*>(g_memory.Translate(address));}
bool UsaInfiniteMeters(bool supportedUsa,bool nitro,bool speedbreaker) {
    if(!supportedUsa)return false;
    constexpr uint32_t guards[][2]={
        {0x82424B78,0x3D6082A3},{0x82424B80,0x88ABCE71},{0x82424B84,0x2B050000},{0x82424B88,0x409A0020},
        {0x82424BA8,0xFFE0F090},{0x82424BE4,0xD1BF00FC},{0x82424C18,0xD1BF00FC},
        {0x8246C224,0x3D4082A3},{0x8246C228,0x888AD1C6},{0x8246C22C,0x2B040000},{0x8246C230,0x409A0058},
        {0x8246C240,0xD01F0084},{0x8246C284,0xD01F0084}};
    for(const auto& g:guards)if(Word(g[0])!=g[1])return false;
    if(Byte(0x82A2CE71)>1 || Byte(0x82A2D1C6)>1)return false;
    *static_cast<uint8_t*>(g_memory.Translate(0x82A2CE71))=nitro?1:0;
    *static_cast<uint8_t*>(g_memory.Translate(0x82A2D1C6))=speedbreaker?1:0;
    return true;
}
}
namespace game::cheats {
void Initialize(bool supportedUsa) {
    bool selected=false,blackEdition=false,nitro=false,speedbreaker=false;
    std::ifstream in(GetUserPath()/"cheats.txt");
    for(std::string line;std::getline(in,line);) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line=="career_cash_payout=1")selected=true;
        if(line=="black_edition=1")blackEdition=true;
        if(line=="infinite_nitro=1")nitro=true;
        if(line=="infinite_speedbreaker=1")speedbreaker=true;
    }
    // Unlimited nitro and SpeedBreaker (Codex's USA handoff, checked against
    // the generated code): two data flags the game's own meter code reads.
    // sub_82424988 loads the byte at 82A2CE71 and, when it is set, uses a
    // zero drain for the nitro charge (this+0xFC); sub_8246C188 loads the byte
    // at 82A2D1C6 and, when set, skips the SpeedBreaker meter's depletion
    // store (this+0x84). Both are 0 in the image. Written before guest code
    // runs, only when the words around those loads are the expected ones.
    if(nitro || speedbreaker) {
        const bool written=UsaInfiniteMeters(supportedUsa,nitro,speedbreaker);
        fprintf(stderr,"[cheats] unlimited nitro selected=%d, unlimited SpeedBreaker selected=%d; eligible=%d active=%d (flags 82A2CE71=%02X 82A2D1C6=%02X)\n",
            nitro,speedbreaker,supportedUsa,written,Byte(0x82A2CE71),Byte(0x82A2D1C6));
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
