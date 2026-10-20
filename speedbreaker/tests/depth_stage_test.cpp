// SpeedBreaker runtime. GPL-3.0-or-later.
#include <gpu/depth_stage.h>
#include <cassert>
#include <iostream>
int main() {
    // Exhaust all observable effects, including a write to each color channel.
    for (unsigned mask=0; mask<65536; ++mask)
        for (unsigned effects=0; effects<32; ++effects)
            assert(gpu::CanOmitFragmentStage(mask,effects&1,effects&2,effects&4,effects&8,effects&16) == (mask==0 && effects==0));
    std::cout << "Depth-stage safety: 2097152 combinations passed\n";
}
