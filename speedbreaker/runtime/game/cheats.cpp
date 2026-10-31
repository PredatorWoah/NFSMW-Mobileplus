// SpeedBreaker One. GPL-3.0-or-later.
// Optional USA-specific cheats: a career payout adjustment and the game's own
// unlimited nitro / SpeedBreaker flags. No raw instruction patch, external
// trainer execution, save editing or arbitrary memory search.
#include <stdafx.h>
#include "cheats.h"
#include <kernel/function.h>
#include <kernel/vmem.h>
#include <user/paths.h>
#include <atomic>
#include <fstream>

extern "C" PPC_FUNC(__imp__sub_82337760);
extern "C" PPC_FUNC(__imp__sub_822E8D48);
extern "C" PPC_FUNC(__imp__sub_82337198);
extern "C" PPC_FUNC(__imp__sub_8256A940);
extern "C" PPC_FUNC(__imp__sub_82335F90);
extern "C" PPC_FUNC(__imp__sub_82424988);
extern "C" PPC_FUNC(__imp__sub_82586768);
extern "C" PPC_FUNC(__imp__sub_82586B80);
extern "C" PPC_FUNC(__imp__sub_822DADB8);
namespace {
bool s_cash=false;
// Unlimited money, all cars, Challenge Series stars (Initialize; read-only after).
bool s_money=false,s_cars=false,s_stars=false;
// 2x nitro, the player's car only (Initialize; read-only after).
bool s_nitro2x=false;
// Sure-shot pink slip and plenty of upgrade markers (Initialize; read-only after).
bool s_pinkSlip=false,s_markers=false;
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
// sub_82424988 is the engine's nitro step (this = the engine, +0xFC the
// charge, +0x104 the boost fading out). It returns the engine's torque factor:
// exactly 1.0 (the constant at 82063038), or while nitro fires 1.0 plus the
// car's nitro boost (its NOS attributes, +0x128 -> +4). The drain skips owners
// whose driver class (vtable +88 of [this+0x48]) is 6, remote; 0 is the human
// player. The same words Codex checked for unlimited nitro, plus the return.
bool NitroCode() {
    return Matches({{0x82424B78,0x3D6082A3},{0x82424B80,0x88ABCE71},{0x82424B84,0x2B050000},{0x82424B88,0x409A0020},
        {0x82424BA8,0xFFE0F090},{0x82424BE4,0xD1BF00FC},{0x82424C18,0xD1BF00FC}}) &&
        Word(0x82063038)==0x3F800000;  // the 1.0 it returns when nitro is off
}
// Rival markers (all from the generated code). The marker store at 82A391B0:
// 63 owned markers {type, value, 1} (12 bytes each), then six pending ones
// (+756) and their count (+828). Types (the name table at 8290D698): 1 brakes,
// 2 engine, 3 nos, 4 induction, 5 chassis, 6 tires, 7 transmission, 8 bodykit,
// 9 hood, 10 spoiler, 11 rims, 12 roofscoop, 13 hud, 14 vinyls, 15 decals,
// 16 paint, 18 pink slip (value: the rival's car), 19 cash. The marker screen
// (sub_82586768, this = the screen) copies the six into cards at this+116
// (type, +4 value, +8 revealed byte; their count at this+44) and on message
// 0C407210 flips the card sub_82586B80 names: 18 adds the car to the garage,
// 19 the cash, others go into the owned markers.
constexpr uint32_t kMarkerStore=0x82A391B0,kFlipCard=0x0C407210,kRivalCarName=0x82076BD8;  // "BL%d"
bool MarkerCode() {
    return Matches({{0x825868B0,0x80DD0074},{0x825868B4,0x2F060012},{0x825868C0,0x2F060013},
        {0x822EADE0,0x3D6082A4},{0x822EADE4,0x394B91B0},{0x822EADEC,0x392A02F4},{0x82076BD8,0x424C2564},
        {0x82586B80,0x7D8802A6},{0x822DADB8,0x7D8802A6}});
}
float Float(uint32_t address) {const uint32_t w=Word(address);float f;memcpy(&f,&w,4);return f;}
void SetWord(uint32_t address,uint32_t value) {*static_cast<uint32_t*>(g_memory.Translate(address))=ByteSwap(value);}
// The engine's owner says what drives it: the guest's own virtual getter,
// called the way sub_82424988 calls it. -1 when anything looks off.
int DriverClass(PPCContext& ctx,uint8_t* base,uint32_t engine) {
    auto inImage=[](uint32_t a){return a>=0x82000000 && a<0x82CD0000-4 && !(a&3);};
    if(!Writable(engine+0x48))return -1;
    const uint32_t owner=Word(engine+0x48);
    if(!Writable(owner))return -1;
    const uint32_t vtable=Word(owner);
    if(!inImage(vtable+88))return -1;
    const uint32_t function=Word(vtable+88);
    PPCFunc* getter=inImage(function)?g_memory.FindFunction(function):nullptr;
    if(!getter)return -1;
    PPCContext call=ctx;  // (the caller's frame is free below r1)
    call.r3.u64=owner;
    getter(call,base);
    return call.r3.s32;
}
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
// mask 1 (completed) for each of its events (call at 8256AAF0, li r5,1 at
// 8256AAE0), and the career check that grants the series' reward asks the
// same (call at 82364C24, li r5,1 at 82364C18).
//
// Answering "completed" alone (0.1.16) only drew the stars: the screen's
// locks (sub_822E8CC8, "beat challenge 3 or 4") and its count (5/69) read the
// records themselves. So for each event the screen asks about, the event's
// own record (the 16-byte entries at [manager+60], [manager+0] of them, the
// id at +0, the flags at +4) gets the game's completed bit, 1, as finishing it
// would; and once the screen is built (sub_8256A940), the game recounts
// (sub_82335F90(manager), its bin counters). Only records the game's own
// lookup already holds are touched: nothing is added past the table. The game
// saves the profile as after a win, so it is kept.
constexpr uint32_t kStarsScreen=0x8256AAF4, kStarsReward=0x82364C28;
constexpr uint32_t kChallengeManager=0x82A2CB1C;  // lwz r3,-13540(r27) in the screen
std::atomic<bool> s_starsMarked{false};
bool StarsCode() {
    // li r5,1 and bl 82337198 at both callers; the query's own loads of the
    // table (lwz r8,0(r3); lwz r11,60(r3)) and its mask test (and r10,r11,r5).
    return Matches({{0x8256AAE0,0x38A00001},{0x8256AAF0,0x4BDCC6A9},{0x82364C18,0x38A00001},{0x82364C24,0x4BFD2575},
        {0x82337198,0x81030000},{0x823371A0,0x8163003C},{0x823371D8,0x7D6A2838},
        // The screen loads the manager [82A2CB1C] (lis r27,-32093; lwz r3,-13540(r27))
        // and the recount takes it in r3 (mflr r12 ... mr r29,r3; lwz r11,32(r29)).
        {0x8256A960,0x3F6082A3},{0x8256A96C,0x807BCB1C},{0x82335F90,0x7D8802A6},{0x82335F9C,0x7C7D1B78},{0x82335FA4,0x817D0020}});
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
        if(line=="nitro_2x=1")s_nitro2x=true;
        if(line=="sure_pink_slip=1")s_pinkSlip=true;
        if(line=="plenty_markers=1")s_markers=true;
    }
    if(s_pinkSlip || s_markers) {
        const bool code=supportedUsa && MarkerCode();
        fprintf(stderr,"[cheats] sure pink slip selected=%d, plenty of markers selected=%d; eligible=%d active=%d\n",
            s_pinkSlip,s_markers,supportedUsa,code);
        s_pinkSlip=s_pinkSlip && code;
        s_markers=s_markers && code;
    }
    if(s_nitro2x) {
        const bool code=supportedUsa && NitroCode();
        fprintf(stderr,"[cheats] 2x nitro (your car) selected=1 eligible=%d active=%d\n",supportedUsa,code);
        s_nitro2x=code;
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
    if(s_markers) {
        // At least two owned markers of each part type (1-16), checked every
        // five seconds; one taken in the shop comes back. Empty slots only,
        // the type written last so a half-written entry is never seen.
        static uint32_t frames=0;
        if(frames++%300==0) {  // (the store is the image's own data, 82A391B0-82A3949C: always there)
            int have[17]={};
            for(uint32_t i=0;i<63;i++) {const uint32_t t=Word(kMarkerStore+i*12);if(t>=1 && t<=16)have[t]++;}
            int added=0;
            for(uint32_t i=0,type=1;i<63 && type<=16;i++) {
                while(type<=16 && have[type]>=2)type++;
                if(type>16)break;
                const uint32_t slot=kMarkerStore+i*12;
                if(Word(slot)!=0)continue;
                SetWord(slot+4,0);SetWord(slot+8,1);SetWord(slot,type);
                have[type]++;added++;
            }
            static int logged=0;
            if(added && logged++<5)fprintf(stderr,"[cheats] plenty of markers: %d part markers added (2 of each type kept)\n",added);
        }
    }
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
    const uint32_t caller=uint32_t(ctx.lr), manager=ctx.r3.u32, event=ctx.r4.u32, mask=ctx.r5.u32;
    __imp__sub_82337198(ctx,base);
    if(!s_stars || (caller!=kStarsScreen && caller!=kStarsReward))return;
    ctx.r3.u64=1;
    if(caller!=kStarsScreen || mask!=1 || !event || !Writable(manager) || !Writable(manager+60))return;
    // This event's record, found as the game's own lookup does (bounded by the count).
    const uint32_t count=Word(manager), table=Word(manager+60);
    if(count>4096)return;
    for(uint32_t i=0;i<count;i++) {
        const uint32_t record=table+i*16;
        if(!Writable(record) || !Writable(record+4))return;
        if(Word(record)!=event)continue;
        const uint32_t flags=Word(record+4);
        if(!(flags&1)) {
            *static_cast<uint32_t*>(g_memory.Translate(record+4))=ByteSwap(flags|1);
            if(!s_starsMarked.exchange(true))fprintf(stderr,"[cheats] Challenge Series: marking events completed (event %08X first)\n",event);
        }
        return;
    }
}
PPC_FUNC(sub_8256A940) {
    __imp__sub_8256A940(ctx,base);
    if(!s_stars || !s_starsMarked.exchange(false))return;
    // The screen marked events: the game's own recount, as after a race.
    const uint32_t manager=Word(kChallengeManager);
    if(!Writable(manager))return;
    const uint64_t r3=ctx.r3.u64,r4=ctx.r4.u64,r5=ctx.r5.u64;
    ctx.r3.u64=manager;
    __imp__sub_82335F90(ctx,base);
    ctx.r3.u64=r3;ctx.r4.u64=r4;ctx.r5.u64=r5;
    fprintf(stderr,"[cheats] Challenge Series: recounted; reopen the screen to see the locks lifted\n");
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
// 2x nitro, the player's car only: the nitro's share of the torque factor
// (result - 1.0) doubled while it fires; every other car, and every frame
// without nitro, keeps the game's own number.
PPC_FUNC(sub_82424988) {
    const uint32_t engine=ctx.r3.u32;
    __imp__sub_82424988(ctx,base);
    const double factor=ctx.f1.f64;
    if(!s_nitro2x || !(factor>1.0) || !(factor<16.0))return;
    const int driver=DriverClass(ctx,base,engine);
    static int logged=0;
    if(logged<6) {
        static uint32_t seen[6];
        bool fresh=true;
        for(int i=0;i<logged;i++)fresh=fresh && seen[i]!=engine;
        if(fresh) {
            seen[logged++]=engine;
            fprintf(stderr,"[cheats] nitro on engine %08X, driver class %d: factor %.3f%s\n",engine,driver,factor,
                driver==0?std::format(" -> {:.3f} (2x)",1.0+2.0*(factor-1.0)).c_str():" (not yours: unchanged)");
        }
    }
    if(driver==0)ctx.f1.f64=1.0+2.0*(factor-1.0);
}
// Sure-shot pink slip: on the first flip of a rival's markers, the card under
// the player's finger becomes the pink slip (swapped with the hidden one, or
// made the way the game makes it, the car "BL<rival>", if none of the six is
// one). Once a pink slip is face up, nothing changes.
PPC_FUNC(sub_82586768) {
    const uint32_t screen=ctx.r3.u32;
    if(s_pinkSlip && ctx.r4.u32==kFlipCard && Writable(screen+44) && Writable(screen+116+6*12)) {
        const uint32_t cards=Word(screen+44);
        PPCContext call=ctx;
        call.r3.u64=screen;
        __imp__sub_82586B80(call,base);
        const uint32_t pick=call.r3.u32;
        auto card=[&](uint32_t i){return screen+116+i*12;};
        if(cards>=1 && cards<=6 && pick<cards && !Byte(card(pick)+8) && Word(card(pick))!=18) {
            int hidden=-1;bool faceUp=false;
            for(uint32_t i=0;i<cards;i++) {
                if(Word(card(i))!=18)continue;
                if(Byte(card(i)+8))faceUp=true;
                else hidden=int(i);
            }
            if(!faceUp) {
                if(hidden>=0) {
                    const uint32_t t=Word(card(pick)),v=Word(card(pick)+4);
                    SetWord(card(pick),18);SetWord(card(pick)+4,Word(card(hidden)+4));
                    SetWord(card(hidden),t);SetWord(card(hidden)+4,v);
                    fprintf(stderr,"[cheats] sure pink slip: card %u swapped with the pink slip (card %d)\n",pick,hidden);
                } else {
                    const uint32_t db=Word(0x82A2C684);
                    if(Writable(db+0x10) && Writable(Word(db+0x10)+184)) {
                        const uint32_t rival=Byte(Word(db+0x10)+184);
                        PPCContext make=ctx;
                        make.r3.u64=kRivalCarName;
                        make.r4.u64=rival;
                        __imp__sub_822DADB8(make,base);
                        if(make.r3.u32) {
                            SetWord(card(pick),18);SetWord(card(pick)+4,make.r3.u32);
                            fprintf(stderr,"[cheats] sure pink slip: no pink slip among %u markers; card %u is now rival %u's car\n",cards,pick,rival);
                        }
                    }
                }
            }
        }
    }
    __imp__sub_82586768(ctx,base);
}
