# RP6 investigation: from 0.1.10 to a credible 720p60 path

Date: October 16, 2026. Scope: detailed investigation and proposed fixes, not a
new APK or a claimed gain. Existing branch android/rp6-bringup; compiled baseline
56d3fb6, signed APK code 11. Preserve ISO loading, audio, all casters, nine-tap
shadow filtering, motion blur and the accepted lighting fixes.

## Decision

Prioritize shader predicate coalescing and genuinely unnecessary fragment stages,
then vertex-fetch routing / hardware input. Develop a guarded sky optimization
separately. Keep lazy guest materialization and CPU replacements behind those
until their dependencies and benefit are proved. Do not sell a resolve-only or
compiler-flag change as the route to a twofold gain.

Our CPU/rasterization paths differ substantially from the faster reference;
0.1.9 adopted only a small subset of its techniques. Keeping this runtime does
not require keeping shader-based vertex fetching or its current shader output.

## Evidence from the supplied 0.1.10 session

The private 11:51:21 session contains 24 sampled frames, 1,145 operation rows,
zero unavailable results and zero dropped regions. There is one front conversion
per sampled frame. The pass pool is valid and producing results on this device.
Per-frame category sums are averages over those sampled frames:

| Operation group | ms/sample frame | Share of recorded operation time |
| --- | ---: | ---: |
| Color rendering, with and without depth | 25.893 | 74.6% |
| Depth-only attachment rendering | 4.966 | 14.3% |
| Depth resolves | 2.107 | 6.1% |
| Color resolves | 1.384 | 4.0% |
| Front conversion | 0.331 | 1.0% |
| Texture compute and transfers combined | 0.024 | <0.1% |

Recorded operation sum: median 34.693 ms; range 24.859–44.059. Ordered timestamp
markers can perturb overlap and pass scheduling: these are elapsed intervals,
not isolated shader cycles or GPU hardware-counter measurements. Their sum is
not a calibrated displayed frame time. Do not add submission_gpu_us per row.

23 non-transition aggregate windows with >=100 complex frames have median window
mean CP-swap 34.850 ms and completed GPU time 33.600 ms/sample. CP-swap p95's median
is 40.587 ms. These are neither a controlled delta against 0.1.9 nor proof of a
fixed 30 FPS limiter. Frame Rate is 60; internal rendering is 720p.

Color-render subdivision:

| Target area / attachment pattern | ms/sample frame |
| --- | ---: |
| 1280x720 with depth | 14.32 |
| 256x256 with depth | 6.83 |
| 640x360 with depth | 3.23 |
| 1280x720 without depth | 1.05 |
| Other color passes | ~0.46 |

Six 256x256 color resolves to distinct destinations occur in sample frames.
Their structure is consistent with cubemap faces; the 640x360 view is consistent
with a mirror. These semantic names are inferences from dimensions/sequence,
not a captured guest render-view identifier. They must not drive skip logic.

Depth target b48 at 1600x1600 appears in 54 pass segments and two depth resolves
per sampled frame. Sample 1603: its first segment records 1,169 draws, only 24
using the synthetic depth-only shader and 74 with API alpha test. The remaining
draws are NOT proven safe to strip: real shader kills/depth exports are not in
this CSV. Other depth segments contain solid caster draws. Preserve them all.

Within fixed target classes, draw count correlates strongly with pass duration:
256x256 color/depth r=0.9939 (157 segments), 1280x720 color/depth r=0.9775 (39),
1600x1600 depth r=0.9907 (54). This motivates geometry/draw-cost investigation;
it does NOT distinguish vertex work, binning, material mix, or fragment work.
Counts of vertices/triangles/invocations are still absent.

The Android presentation defect is fixed: creates stays 1 despite >4,400
SUBOPTIMAL reports; policy rebuild time stays zero. Home produces focus loss,
[suspend], pause/stop and no continued render logs. CPU dumpsys spans include
foreground time, so it is not an isolated background CPU percentage.

Async is ON throughout; no on/off comparison was made. 129 adopted pipeline
rows: median individual compile 0.629 ms, maximum 6.936 ms; maximum individual
CP wait 6.959 ms. Warm hitch rows after 11:52:03: only 1 of 136 includes pipeline
creation/wait. Individual waits must not be confused with a frame's sum.
Among those hitch rows only, CP actual core time median is 14.4 ms, p95 20.9 ms,
max 25.2 ms. This is a biased hitch subset, not the whole race's CPU distribution,
but shows that GPU improvements can expose a CPU limit before locked 60.

## Reference and comparison settings

Inspected native implementation codepdbh/nfsmw-android pinned to
6df1501974d0db84893f5a3ca465b616414f6961, the native engine used by the supplied
victorgbd v0.4.2 reference, plus wrapper tag v0.4.2. Do not substitute current main.

Wrapper Ajustes.java/MotorNativo.java maps xbox360 shadows to pcf_barato=false
and sombras_sin_vegetacion=false; motion blur on maps sin_desenfoque=false.
Thus those user-selected options really request the costly paths. Engine source
defaults alone would incorrectly suggest the test removed trees and blur.
No new capture of that port proves the precise scene, clocks or visual parity.

Concrete differences:

| Area | SpeedBreaker 0.1.10 | Pinned native reference |
| --- | --- | --- |
| Vertex input | Empty VkPipelineVertexInputState; shader fetches storage words, redirects, swaps, decodes | VkVertexInputBinding/AttributeDescription; endian-correct uploads; hardware attribute fetch |
| Predicates | Per-instruction if(p0), plus CF predicate blocks | Generation-aware coalescing, including adjacent matching CondExecPred |
| No-color draws | No fragment stage only for synthetic depth-only PS | Also strips a real PS when no discard/depth side effect needs it; alpha-only variant otherwise |
| Sky | Guest order retained | Guarded deferred sky draw; identity + geometry + state and order checks |
| Resolves | Fused texture AND guest tiled stores | Image swaps, versioned/deferred copies, limited proven guest materialization |
| CPU front end | Mostly translated PPC, conservative register-local options | Guarded native visibility/material/matrix/effect helpers and other build work |

Source links at pinned reference:
- [Draws, input assembly, sky and depth paths](https://github.com/codepdbh/nfsmw-android/blob/6df1501974d0db84893f5a3ca465b616414f6961/app/src/nfsmw_nativo_dibujos.cpp)
- [Target copies and lazy materialization](https://github.com/codepdbh/nfsmw-android/blob/6df1501974d0db84893f5a3ca465b616414f6961/app/src/nfsmw_nativo_destinos.cpp)
- [Predicate emitter](https://github.com/codepdbh/nfsmw-android/blob/6df1501974d0db84893f5a3ca465b616414f6961/shaders/XenosRecomp/shader_recompiler.h)
- [Wrapper settings mapping](https://github.com/victorgbd/NFSMW-Recompiled-Mobile/blob/v0.4.2/android/app/src/main/java/io/github/nfsmwrecomp/MotorNativo.java)

## Confirmed offline compiler result

Built glslang 15.1.0 and its pinned SPIR-V tools on the host. Used common GLSL
from the actual synthetic TranslateShader test, then two explicit nine-sample
predicate probes. Both use the same uniform predicate, same coordinates, same
nine explicit-LOD samples and accumulation order. -Os -g0 matches Android's
size-optimization/strip settings. This tests compiler discrimination, not a
reconstructed hot game shader or final Adreno machine code.

| Probe | Vulkan 1.2 words | Conditional branches | Texture samples |
| --- | ---: | ---: | ---: |
| Nine separate same-predicate blocks | 886 | 9 | 9 |
| One shared predicate block | 575 | 1 | 9 |
| vmax(a,a) through existing helper | 271 | 0 | 0 |
| Direct a | 224 | 0 | 0 |

Vulkan 1.1: respectively 884/573/270/223 words, same branches and samples.
All eight modules compile and validate. The assumption that our optimizer always
merges repeated predicates is falsified for this pattern. Actual hot-shader
incidence and RP6 cycle savings remain unmeasured. Word-count reduction is not an
FPS prediction. The max-copy result is secondary: current helpers differ from
native fmax, so native claims about FMul are not ours; do not simplify it without
NaN, infinity, signed-zero, denormal and clamp regression checks.

Reproduce after generating synthetic GLSL:

```sh
clang++ -std=c++20 -O2 -Iruntime tests/shader_variants_test.cpp \
  runtime/gpu/shader_translator.cpp runtime/gpu/xenos/ucode.cpp -o /tmp/sb-shaders
/tmp/sb-shaders /tmp/sb-shader-probes
python3 scripts/analysis/probe_predicate_codegen.py /tmp/sb-shader-probes \
  --glslang /path/to/glslang --dis /path/to/spirv-dis --val /path/to/spirv-val
```

## Proposed implementation sequence

### 1. Predicate coalescing: first translator change

Touch shader_translator.cpp Translator::Alu, VertexFetch, TextureFetch, Exec,
EmitFlatCf and EmitStateMachine. Represent an open predicate block explicitly.
Keep adjacent instructions under the same predicate only while its p0 generation
is unchanged. Close on predicate writes (including paired scalar operations),
condition changes, unpredicated instructions and every control-flow boundary.
Do not assume a surrounding CondExecPred stays valid after p0 is written inside.
Do not move texture samples across instructions, change LOD/derivatives, remove
PCF taps or regroup floating sums. Never merge across branch/call/loop targets.

Validation: synthetic instruction streams with matching/opposite predicates,
ALU and fetch mixtures, predicated p0 updates, scalar/vector combined writes,
conditional EXEC termination, loops/calls; compare governing predicate generation
for each original instruction. Compile/validate the real emitted variants under
both target environments, verify sample counts and texture offsets. Preserve the
old emitter behind a restart diagnostic option; shader source hash changes must
invalidate cache identity automatically. Acceptance: improved warmed affected
pass time with unchanged shadows/alpha/derivatives; a neutral result is possible.

### 2. General safe no-fragment / depth-only variants

Touch renderer.cpp PipelineKey, BuildPipeline, Draw, VertexModule and pixel
variant output masks. Existing no-fragment bit only accepts kDepthOnlyPsHash.
Broaden only when every color write is disabled, API alpha test is ineffective,
PS has no kill, depth export, memexport or sample-mask side effect, and no other
observable fragment effect is omitted. Retain depth/stencil state, bias, culling,
queries and all geometry. If driver creation fails, fall back to the old stage.

For alpha-tested no-color draws, compile outputs=0 even if a dormant color
attachment remains bound; keep kill/alpha/depth behavior. Already no-color
attachments use OUT macros, so this is not a new gain for every depth pass.

Build a true position-only VS variant for passes with no fragment consumers.
Do not merely leave ten unused interpolators exported. Pipeline key/module cache
must encode output interface; retain invariant gl_Position and identical math.
Check whether the driver already eliminates outputs before attributing a gain.
Existing CSV cannot quantify extra eligible draws: record eligibility/rejection
counts in the first implementation build, not a separate diagnostic-only APK.

Validation: exhaustive color mask/alpha/kill/export combinations; depth/stencil
and occlusion regression, shader/interface validation, unchanged vegetation and
car/tunnel shadows. Upper affected region is the ~5 ms depth rendering category,
not a promise to save all of it. Other no-write query draws also need audit.

### 3. Vertex fetch: staged reduction, then hardware path

Confirmed path: ShaderCommonGlsl::vmem tests up to four snapshot ranges for each
word before shared-memory fallback; VertexFetch reads dynamic endian, applies
swap and decodes packed/normalized fields. Pipeline vertex input is empty.
Reference CrearPipelineVulkan binds real formats/strides and byte-correct buffers.

First smaller change: precompute a direct source route per fetch constant using
existing snapshot ownership. A fetch whose whole declared range is in a snapshot
can use that exact ring base; a guarded shared range uses its known route. Retain
fallback for partial/aliased/GPU-owned data. Specialize fixed endian/format only
where cache key/constant identity proves it, rather than compile a variant for
every buffer address. This can remove range selection without changing lifetime.

Then hardware input for a conservative eligible subset: ordinary indexed draws,
static fetch layout and index from the original vertex ID. Keep skinning,
transforms and game shader ALU. Convert endian once into a versioned upload/cache,
map supported SNORM/UNORM/float formats into VkFormat and use binding/attribute
state. Unsupported formats, modified/relative vertex indices, unusual mini
fetches, rectangle expansion and GPU-written ranges retain shader fetching until
proved separately. SNORM extrema, no-zero normalization, packed formats, stride,
base vertex, restart and fetch swizzles must match guest semantics explicitly.

Cache identity includes range, byte order/layout and actual write generation;
retire offsets only after all referencing fences. Guest waits and writes must
invalidate safely, not just a frame counter. Use existing write-watch/ownership
as authority. A reference CPU decode of randomized and boundary inputs should
match captured/prepared attributes before enabling. Measure added CPU prepare
cost: it can erase a GPU saving with the current CP headroom. Share decoded
ranges across shadow/main/offscreen views when versions/lifetimes permit.

This targets all three expensive rendering groups and is the strongest larger
architectural candidate, not a proved vertex bottleneck or promised multiplier.

### 4. Sky/overdraw: separate guarded work

Reference identifies the sky by original container fingerprint, 480-index dome,
opaque blend, read-only LEQUAL depth, no stencil/query, and replays before any
incompatible consumer/pass closure. Its earlier broad shader-only match corrupted
other draws sharing the shader. That warning applies directly here.

Our hashes are of different representations: copying 28AA3CDAC6C19705 into our
pipeline lookup would not identify the same program. Establish container-to-
microcode mapping from the user's assets or verified shader creation hook first;
last_ps_hash in this CSV is neither a sky identifier nor per-draw cost.

Retain constants, descriptors, vertex/index versions and resource lifetimes until
replay. Flush pending work before blending, depth-state transitions, queries,
resolves, target clears/reuse and submit boundaries. Far-plane equal-depth ties
can change reorder results even with LEQUAL; prove their handling or reject the
case. Do not call arbitrary sky-last reordering pixel-identical. A depth prepass
is an alternative experiment with more vertex/draw cost, not a free fix.

Acceptance: matched image comparisons including horizon, tunnels, reflections,
weather and sky glow, plus lower sampled scene time. Reference milliseconds on
NVK/Switch are not an RP6 estimate.

### 5. Resolve residency and CPU work after the above

Resolves total ~3.49 ms, including two ~0.89 ms 1600x1600 depth conversions each
frame. Useful secondary work, insufficient by itself to halve the workload.
Before eliminating a guest store, track exact producer version, texture readers,
CPU exposure reads, vertex/index consumers, partial overlaps, endian/packing,
write-watch publication, resolve aliases and source destruction. Materialize on
the first legitimate guest consumer, not never. Current fused stores already
avoid separate texture upload copies; do not count that saving twice.

The reference's split world/car depth minimum trick is tied to cheap single-tap
PCF and is not transferable unchanged to nine-tap Xbox filtering. Keep it out of
the first batch. Likewise, our shadow LOADs after resolves/submission breaks are
not automatically redundant: they preserve earlier casters. First segments
already CLEAR. Neither blanket DONT_CARE nor fewer cubemap updates is an exact
optimization.

CPU: measure guest/CP core time across all frames separately from waits. Prioritize
same-version snapshot/decode reuse and redundant state/constant preparation if
hot. Guarded material/matrix/effect helpers come after matching function identity
and PPC side effects (memory, FP status, registers, indirect entries, longjmp).
The reference matrix/material anchors sub_824538D0 and sub_824511E8 are absent
from our generated mapping. Do not use those addresses as hooks in this build.
Its native helpers also rely on local volatile-register liveness that differs
from our conservative context-backed setup. Match the actual functions and
compare full observable state first. Our generated direct calls already exist; adding another indirect-call shortcut
is not automatically useful. Register-local toggles in config/nfsmw.toml are off;
changing them globally without secondary-entry/unwind tests is unsafe. RP6-trained
PGO/ThinLTO are later options; do not import Switch PGO data or driver patches.

## Next build and acceptance contract

No new APK is built by this investigation. Next implementation should combine
one installable test build with independently reversible predicate and depth-stage
changes, then vertex routing if its host equivalence tests pass. Retain pass
sampling and add eligible-draw counts, vertices/indices, first/last shader metadata
and snapshot-route counts. Pipeline statistics only if supported and explicitly
enabled at device creation; currently not enabled. Never nest statistics queries
of the same type or infer fragment invocations from draw count. Include hot
shader summaries, not blanket shader/game-data dumps. No extra identical 0.1.10
capture is needed to begin implementation.

Use same warmed race/car, graphics and thermal/power settings; fixed scheduling;
record rollback modes and current shader preference. Measure per-change windows,
pass time, CP core time, complete-frame percentiles and visual/audio correctness.
Discard startup, mode transitions and instrumented-frame artifacts as appropriate.
A smaller SPIR-V module alone is not a performance result.

60 FPS budget is 16.667 ms for the complete frame, with presentation/variance
headroom. Current ~33.6 ms completed GPU aggregate needs about a twofold reduction;
CPU tails must fit too. A 30% GPU reduction would still leave ~23.5 ms before
other frame costs. Require sustained warm racing, frame-time tails and a second
race; an average near 60 or a menu counter does not establish locked 60.
