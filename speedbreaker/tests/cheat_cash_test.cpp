#include <game/cheats.h>
#include <cassert>
#include <limits>
#include <iostream>
int main(){
 using game::cheats::CareerCashReward;
 for(uint32_t cash:{0u,1u,12500u,99999999u,100000000u,100000001u,2147483647u,0xffffffffu})
  for(double reward:{0.,.99,1.,12500.,100000000.,-1.,double(std::numeric_limits<int32_t>::max()),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
   double got=CareerCashReward(cash,reward);
   if(cash<=100000000 && std::isfinite(reward) && reward>=0 && reward<=double(INT32_MAX)-cash) {
    assert(cash+std::trunc(got)>=100000000);
    assert(cash+std::trunc(got)<=INT32_MAX);
    if(cash+std::trunc(reward)>=100000000)assert(got==reward);
   }else assert(std::isnan(reward)?std::isnan(got):got==reward);
  }
 uint32_t replacement=0xDEADBEEF;
 assert(game::cheats::BlackEditionFlag(true,0,replacement) && replacement==0x100);
 assert(game::cheats::BlackEditionFlag(true,0x100,replacement) && replacement==0x100);
 for(uint32_t original:{1u,0x101u,0xffffffffu}) {
  replacement=0xDEADBEEF;assert(!game::cheats::BlackEditionFlag(true,original,replacement));assert(replacement==0xDEADBEEF);
 }
 replacement=0xDEADBEEF;assert(!game::cheats::BlackEditionFlag(false,0,replacement));assert(replacement==0xDEADBEEF);
 std::cout<<"72 payout boundary cases and 6 edition guard cases passed\n";
}
