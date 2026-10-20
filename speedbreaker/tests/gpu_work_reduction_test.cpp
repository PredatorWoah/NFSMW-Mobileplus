// SpeedBreaker. GPL-3.0-or-later. Boundary checks for image-preserving paths.
#include <gpu/early_depth.h>
#include <platform/android/lifecycle_policy.h>
#include <cassert>
#include <set>
#include <utility>
#include <cstdio>
int main() {
  // Exercise all Z/stencil/write combinations, shader side effects and queries.
  for(unsigned control=0;control<256;control++)
    for(unsigned flags=0;flags<16;flags++) {
      bool attached=flags&1, writes=flags&2, mem=flags&4, query=flags&8;
      const bool expected=attached && (control&2) && !(control&5) && !writes && !mem && !query;
      assert(gpu::SafeEarlyDepth(control,attached,writes,mem,query)==expected);
    }
  // Compare complete old/new dispatch domains, including partial groups.
  for(unsigned w: {1u,7u,8u,31u,32u,33u,63u,64u,65u,1600u})
    for(unsigned h: {1u,2u,3u,7u,8u,9u,33u}) {
      std::set<std::pair<unsigned,unsigned>> seen;
      for(unsigned by=0;by<(h+1)/2;by++) for(unsigned bx=0;bx<(w+31)/32;bx++)
        for(unsigned l=0;l<64;l++) {
          unsigned x=bx*32+((l&3)|((l>>3)<<2)),y=by*2+((l>>2)&1);
          if(x<w && y<h) assert(seen.emplace(x,y).second);
        }
      assert(seen.size()==w*h);
      for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++) assert(seen.count({x,y}));
    }
  // Android focus can arrive before foreground, and vice versa; duplicates too.
  for(int order=0;order<2;order++) {
    platform::android::LifecyclePolicy p;
    assert(p.Active()); assert(!p.Focus(false)); assert(!p.Background(true));
    assert(!p.Background(true));
    if(order) { assert(!p.Focus(true)); assert(!p.Active()); assert(p.Background(false)); }
    else { assert(!p.Background(false)); assert(!p.Active()); assert(p.Focus(true)); }
    assert(p.Active()); assert(p.Focus(true)); assert(p.Background(false));
  }
  puts("early-depth boundaries, exact resolve coverage and lifecycle ordering passed");
}
