// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
namespace game::cheats {
constexpr uint32_t kCareerCashTarget=100000000;
// The USA caller truncates reward to int32 then adds it to current cash.
// Alter only ordinary positive payouts below the target; preserve other cases.
inline double CareerCashReward(uint32_t current,double reward) {
    if(current>kCareerCashTarget || !std::isfinite(reward) || reward<0 ||
       reward>double(std::numeric_limits<int32_t>::max())-current)return reward;
    return double(current)+std::trunc(reward)<kCareerCashTarget ? double(kCareerCashTarget-current) : reward;
}
inline bool BlackEditionFlag(bool supportedUsa,uint32_t original,uint32_t& replacement) {
    if(!supportedUsa || (original!=0 && original!=0x100))return false;
    replacement=0x100;return true;
}
void Initialize(bool supportedUsa);
// The game's render thread, each frame (VdSwap): unlimited money's top-up.
void OnFrame();
}
