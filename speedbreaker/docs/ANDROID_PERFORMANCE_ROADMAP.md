# Android performance roadmap — 0.1.6

## Baseline and evidence limits

Target: RP6/Adreno; source baseline `5f057d5` (0.1.5 diagnostic), previous
performance trial `087c919` (0.1.4), audio baseline `0acaa1b` (0.1.3).
User reports ~21–39 FPS at internal 1280x720, ~6–9 W across reported sessions,
versus ~50–60 FPS at 1080p in another ReXGlue Android port. These are observations,
not matched frame-time/power benchmarks. The other port establishes feasibility
for another implementation, not this renderer's bottleneck or attainable power.
21–39 FPS corresponds to ~47.6–25.6 ms/frame; 60 FPS allows 16.67 ms.

Existing sanitized checkpoints report ~2,300–3,550 draws, ~20–21 passes/resolves
and command-processor busy wall time ~11–16 ms/frame, with completion waits and
little steady pipeline-wait/upload activity. Those summaries are retained evidence;
raw logs were not reanalyzed for this release. No post-0.1.4 timestamp log or
0.1.5 capture has been supplied/analyzed here. No CPU/GPU utilization distribution,
thermal trace, per-pass attribution or calibrated power comparison is available.

Read `ENGINEERING_METHODOLOGY.md` before acting on this ranking. No additional
performance policy or shader change is made in 0.1.6: the measurements do not yet
justify one. Keep lighting results attributable to the depth fix.

## Trace and counter semantics

Guest PPC render frontend -> PM4 command processor -> `Draw`/`Resolve` -> recorded
Vulkan work -> `SubmitRecorded` -> GPU -> completion thread -> guest fences and
presenter. Source: `runtime/gpu/command_processor.cpp`, `renderer.cpp`,
`runtime/kernel/write_watch.cpp`, `runtime/video/presenter.cpp`.

* `BeginCmd` waits for reusable submission slots; `RingAllocate` can flush on ring
  pressure. `WaitForSubmission` also protects `EnsureShadow`/`SyncShadow` memory
  ownership. These are producer/consumer dependency waits, not shader timings.
* Completion-thread `vkWaitForFences` can wait while GPU work finishes **or** while
  guest/CP/driver supply work. Removing fences would violate buffer reuse and guest
  publication semantics. Trace the critical path before altering them.
* Existing `NFSMW_GPU_TIMING=1` defaults on Android. TOP/BOTTOM timestamps in each
  submission cover elapsed GPU work including barriers, copies and scheduling;
  `[perf] executing` is not shader-only utilization. Query failures are not zero cost.
  Queue-family valid bits, timestampPeriod and wraparound need checking if data looks
  impossible. Compare elapsed intervals with wall time; do not add overlapping
  presenter/renderer intervals or assume all counters describe the same frame.
* `[perf] CPU busy` derives from CP wall-clock accounting minus categorized waits;
  scheduler preemption and unclassified waits remain. It is not sampled thread CPU
  time, and excludes much of the recompiled guest frontend. Existing `[hitch]`
  records also contain CP thread CPU time (`on a core`) and preemption counts;
  inspect those before requesting new sampling.
* Visual capture readbacks end passes, split submissions and wait for completion.
  The three selected capture frames and surrounding windows are contaminated.
  Capture CSV GPU/gap times are useful for structure; they are not steady benchmarks.
* `[untiled-area]` pixel/MB estimates model attachment traffic; they are not hardware
  bandwidth counters. Resolve count alone does not establish redundant work.

## Ranked competing hypotheses and minimal experiments

| Priority / status | Evidence and falsification | Next isolated action / risk / acceptance |
| --- | --- | --- |
| 1: synchronization, submission starvation or slot pressure — suspected | Completion waits in checkpoints; four Android slots and 64 MiB ring. `SubmitIfStarving`, `EnsureShadow`, slot reuse and queue-lock timing already exist. GPU timestamps much shorter than frame time with empty queue would falsify continuous GPU saturation; long continuously supplied GPU intervals weaken pure guest-CPU explanation. | First obtain existing `[perf]` and `[hitch]` after capture finishes; use `[latency]` only if its existing opt-in is enabled in a later controlled session. Check queue-empty fraction, submission sites, slot vs shadow waits and actual ring pressure. No new pool/capture. Increasing slots can increase RAM/latency; removing protection is rejected. Require unchanged guest fences, audio and two-race stability. |
| 2: EDRAM attachment/resolve traffic and conservative barriers — suspected | ~21 resolves/pass transitions; targets can exceed 720p. `Resolve` writes tiled guest memory and often cached textures. Full barriers can reduce overlap, but passes may be semantically required. | Attribute passes using existing diagnostics first; use existing render-area/barrier overrides separately only after hazards are mapped. Validate mirrors, reflection faces, depth, clears, HUD and resolve destinations. Reject deleting apparently unused targets based only on copy counters. |
| 3: translated shader cost and guest memory fetches — suspected; constants optimizations already implemented | Dynamic uniform buffer, packed referenced constants, dirty/submission/pair keyed uploads, binding caches and SPIR-V optimization already exist. `shader_translator.cpp` has `textureSize` calls and explicit guest-memory fetches; their racing hotness/cost is unknown. | Identify hot shader/pass from existing capture and inspect emitted arithmetic before an isolated synthetic test. Consider redundant size queries/predicated blocks only if actually present in hot output. Require shader validation and unchanged sampling, derivatives, depth and alpha behavior. No PCF simplification or caster removal. |
| 4: guest PPC frontend/main thread — plausible, unmeasured | Config keeps register-local transformations off. Android recomp uses optimized RelWithDebInfo but no explicit ARM tuning, IPO or PGO; PPC context loads and helper calls can be expensive. CP busy doesn't measure the guest main thread. | If existing timings show underfed GPU, take one thread sampling profile on a later authorized device session. Inspect hot functions and calling/register-sharing invariants before considering local-register transforms, native helpers or ThinLTO. Risks: secondary entries, longjmp, hooks, memory ordering and code size. PGO needs representative RP6 data; do not reuse Switch profiles. |
| 5: repeated compilation/upload or presentation recreation — weakened for steady racing | Checkpoints report negligible warmed pipeline waits/few uploads; Android SUBOPTIMAL recreation fix already implemented. Queue-idle calls belong to setup/resize/teardown, not every draw. | Recheck warmed logs before reopening. New failures, changing shaders or resize churn would revive this hypothesis. Do not reimplement existing caches or suppress legitimate lifecycle synchronization. |

## Optimizations already present; do not repeat

`renderer.cpp` already contains draw/state caching; pass merging with overlap
checks; untiled guest-view areas with expansion/resolve fallbacks; fused
resolve-to-texture updates plus guest-memory writes; texture/write-sequence
tracking; guarded/snapshotted vertex data; dynamic uniform constants with packed
maps and reuse; starvation submissions; upload-ring slots; optional narrower
pass barriers; and GPU timestamps. Android 0.1.4 enables the guarded area policy;
user reports only a modest gain, without a controlled numerical measurement.
Narrower barriers remain platform-conditioned/opt-in, not an Android default.

## nfsmw-nx comparison

Inspected `StevensND/nfsmw-nx` source/documentation at the commit recorded in
`ANDROID_UPSTREAM_016.md`. Its native Vulkan/NVK/Horizon backend and ReXGlue
code generator differ from SpeedBreaker's XenonRecomp runtime. Transfer concepts
and tests, not backend assumptions or claimed milliseconds.

| Technique | Our state / useful next step |
| --- | --- |
| Eliminate Xbox tiling and reuse resolved images | Pass merging/untiled area/fused texture stores already exist. Retaining guest resolves is required wherever guest code or another resource consumes them; prove consumers and invalidation before zero-copy changes. |
| Dynamic uniform constants, cached uploads and state | Already implemented; profile residual CPU packing/translation cost rather than add another cache. Inverse-size hoisting is a candidate only after checking texture instructions, dimensions and LOD semantics. |
| Driver ZCULL and NVK draw/compiler patches | NVIDIA/Horizon-specific; cannot apply to stock Adreno. Shader early-depth eligibility can be investigated independently while preserving discard/depth exports and occlusion. |
| Cheaper PCF, remove vegetation casters, skip attachment loads | Quality/correctness risks. Caster removal rejected by our requirements. Load-op changes need proof of full overwrite and preservation of depth outside affected regions. |
| Direct guest calls, local registers, LTO/PGO/function ordering | Different generator and hook semantics. Audit ours first. Register-local options are currently off; changing them needs secondary-entry/register-sharing tests. No representative RP6 PGO profile exists. |
| Guarded native hot-function replacements | Defer until sampling identifies hot guest functions; run reference/native comparisons before trusting replacements. |
| Measurement discipline and negative results | Adopt now: CPU time versus busy wall time, warm intervals, control noise, frame distributions, effective settings, observed attempts. Do not apply its 1.627 timestamp calibration to Adreno. |

## Rejected or failed explanations to preserve

* Missing sunlight is not established as an Adreno-only fault: desktop shows it,
  and the shared far-plane pack provably wraps. Upstream fix addresses that.
* Disabled hardware sampler comparison alone is not a shadow bug: translated
  shaders can compare sampled depths explicitly.
* D24FS8 UNORM inconsistency and missing `gl_FragDepth` are concrete code issues,
  but affected shader/format usage remains unknown; neither was proven to cause
  the reported sunlight defect or flicker. No speculative correction.
* Loose-file misses do not prove missing ISO assets; packed paths can succeed.
* GPU fence waits alone do not prove GPU saturation; 60 FPS menu proves neither
  racing GPU capacity nor guest-thread capacity.
* Swapping to another port's entire renderer, dropping shadows or simultaneously
  changing barrier/area/CPU policy would obscure causality and risk correctness.

## Single most valuable next test

Install 0.1.6 over the retained build without clearing data. Use the same Quick
Race/car/track, internal 1x 720p and unchanged performance mode. Drive for two
minutes: first check sunlit roads/car and stable projected shadows; note any
flicker and its time. Allow the bounded three-frame captures to finish. Judge
steady FPS/frame times only afterwards, with warm caches and equivalent thermals.
Check engine audio/music, mirror/HUD and start a second race. One private archive
of the existing session logs and capture is sufficient; no Career progression.

Interpret `[perf]` executing, CP busy and classified waits together; if those
cannot distinguish GPU saturation from starvation, the next experiment is a
short thread sampling profile, not another speculative optimized APK. A 60 FPS
or watts claim requires matched measurement; this release establishes no gain.
