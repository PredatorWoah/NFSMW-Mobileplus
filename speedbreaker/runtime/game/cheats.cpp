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
extern "C" PPC_FUNC(__imp__sub_822E8D48);
extern "C" PPC_FUNC(__imp__sub_82337198);
namespace {
bool s_cash=false;
// Unlimited money, all cars, Challenge Series stars (Initialize; read-only after).
bool s_money=false,s_cars=false,s_stars=false;
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
bool Matches(std::initializer_list<std::pair<uint32_t,uint32_t>> words) {
    for(const auto& [address,word]:words)if(Word(address)!=word)return false;
    return true;
}
// A 4-byte guest word the allocator has committed read/write: a pointer read
// from the game is checked before it's followed or written.
bool Writable(uint32_t address) {
    if(!address || (address&3) || address>UINT32_MAX-4)return false;
    vmem::PageHeap* heap=vmem::HeapFor(address);
    vmem::RegionInfo info{};
    return heap && heap->Query(address,&info) && info.state==vmem::X_MEM_COMMIT &&
        (info.protect&(0x04|0x40)) && uint64_t(address)+4<=uint64_t(info.baseAddress)+info.regionSize;
}
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
// Unlimited money (Codex's USA handoff, checked against the generated code):
// the dealer's purchase at 82565830 loads the career database [82A2C684],
// its profile at +0x10, CareerSettings at +0xB0 and the cash at +0x0C, and
// zeroes the cash when a cost is above it. The race payout (82364884) and the
// customization shop (8256BEA0) reach the same word. Each frame the cash is
// brought back up to 100,000,000 (higher is kept); the game's own code does
// every subtraction and save.
constexpr uint32_t kCareerDatabase=0x82A2C684, kCashTarget=100000000;
bool MoneyCode() {
    // lwz r11,-14716(r26); lwz r11,16(r11); addi r11,r11,176; lwz r10,12(r11)
    return Matches({{0x82565830,0x817AC684},{0x82565834,0x816B0010},{0x82565838,0x396B00B0},{0x8256583C,0x814B000C}});
}
// All cars: the car query sub_822E8D48 ORs the global unlock byte 82A2CE00
// (loaded at 822E8D60) into its normal unlock test; bonus cars later return
// their own conditions, so the query's answer is forced too.
bool CarCode() {
    return Matches({{0x822E8D48,0x7D8802A6},{0x822E8D54,0x3D6082A3},{0x822E8D60,0x8BABCE00},{0x822E8DD0,0x8BDC00B8},
        {0x822E8DD4,0x88A60059},{0x822E8DD8,0x7F05F040},{0x822E8DE4,0x57A3063E},{0x822E8DEC,0x7C6B5B78},{0x822E8DF8,0x557D063E}});
}
// Challenge Series stars: sub_82337198(manager, event, mask) answers whether
// an event's flags have the mask; the Challenge Series screen asks it with
// mask 1 (completed) for each event (call at 8256AAF0, li r5,1 at 8256AAE0),
// and the career check that grants the series' reward asks the same (call at
// 82364C24, li r5,1 at 82364C18). Only those two callers get "completed":
// nothing is written into the event records.
constexpr uint32_t kStarsScreen=0x8256AAF4, kStarsReward=0x82364C28;
bool StarsCode() {
    // li r5,1 and bl 82337198 at both callers; the query's own loads of the
    // table (lwz r8,0(r3); lwz r11,60(r3)) and its mask test (and r10,r11,r5).
    return Matches({{0x8256AAE0,0x38A00001},{0x8256AAF0,0x49CC66A9},{0x82364C18,0x38A00001},{0x82364C24,0x4BFD2575},
        {0x82337198,0x81030000},{0x823371A0,0x8163003C},{0x823371D8,0x7D6A2838}});
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
        if(line=="unlimited_money=1")s_money=true;
        if(line=="unlock_all_cars=1")s_cars=true;
        if(line=="challenge_stars=1")s_stars=true;
    }
    const bool money=s_money,cars=s_cars,stars=s_stars;
    s_money=money && supportedUsa && MoneyCode();
    s_cars=cars && supportedUsa && CarCode();
    s_stars=stars && supportedUsa && StarsCode();
    if(s_cars) {
        uint8_t* flag=static_cast<uint8_t*>(g_memory.Translate(0x82A2CE00));
        if(*flag<=1)*flag=1;
        else s_cars=false;
    }
    if(money || cars || stars)
        fprintf(stderr,"[cheats] unlimited money selected=%d active=%d; all cars selected=%d active=%d (82A2CE00=%02X); "
            "Challenge Series stars selected=%d active=%d; eligible=%d\n",
            money,s_money,cars,s_cars,Byte(0x82A2CE00),stars,s_stars,supportedUsa);
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
namespace game::cheats {
void OnFrame() {
    if(!s_money)return;
    const uint32_t db=Word(kCareerDatabase);
    if(!Writable(db+0x10))return;
    const uint32_t profile=Word(db+0x10), cash=profile+0xBC;
    if(!Writable(cash))return;
    const uint32_t now=Word(cash);
    if(now>=kCashTarget || now>0x7FFFFFFF)return;
    *static_cast<uint32_t*>(g_memory.Translate(cash))=ByteSwap(kCashTarget);
    static int logged=0;
    if(logged++<3)fprintf(stderr,"[cheats] unlimited money: cash %u -> %u\n",now,kCashTarget);
}
}
PPC_FUNC(sub_822E8D48) {
    __imp__sub_822E8D48(ctx,base);  // its attribute cleanup runs as usual
    if(s_cars)ctx.r3.u64=1;
}
PPC_FUNC(sub_82337198) {
    const uint32_t caller=uint32_t(ctx.lr);
    __imp__sub_82337198(ctx,base);
    if(s_stars && (caller==kStarsScreen || caller==kStarsReward))ctx.r3.u64=1;
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
