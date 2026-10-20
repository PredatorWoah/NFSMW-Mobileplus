// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
// Standalone capture limits/gating checks; no Vulkan or game assets.
#include "gpu/visual_capture.h"
#include <cassert>
#include <iostream>
int main(int argc, char** argv) {
    assert(argc == 2);
    setenv("NFSMW_VISUAL_CAPTURE", "1", 1);
    gpu::visual::Capture c;
    for (unsigned i=0;i<100;++i) assert(!c.arm(i,500,20,2,argv[1]));
    for (unsigned i=0;i<29;++i) assert(!c.arm(i,2000,20,2,argv[1]));
    assert(c.arm(30,2000,20,2,argv[1]));
    assert(c.write("probe.bin", "data",4));
    c.bytes=c.limit-2;
    assert(!c.write("oversize.bin","data",4));
    assert(!std::filesystem::exists(c.directory/"oversize.bin"));
    c.finish();
    assert(std::filesystem::exists(c.directory/"manifest.json"));
    for(unsigned capture=1;capture<3;++capture) {
        for(unsigned i=0;i<60;++i) assert(!c.arm(i,2000,20,2,argv[1]));
        assert(c.arm(100+capture,2000,20,2,argv[1])); c.finish();
    }
    for(unsigned i=0;i<100;++i) assert(!c.arm(i,2000,20,2,argv[1]));
    std::cout<<c.directory<<'\n';
}
