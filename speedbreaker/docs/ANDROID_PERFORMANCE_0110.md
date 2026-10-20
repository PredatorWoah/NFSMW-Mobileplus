# Android 0.1.10: pass timing and persistent swapchain reports

## Evidence and source identity

Baseline 0.1.9, clean compiled commit 7cc2e0e, same existing Android branch.
Juan's private Oct 16 11:22:31 capture confirms work reduction ON, overlap policy
2, 1280x720 internal rendering and 1920x1080 presentation on Adreno 740.
57 non-transition windows with at least 100 complex frames: median window mean
CP-swap 36.2859 ms, median completed submission GPU time 34.997 ms/sample.
These are asynchronous aggregates, not displayed-frame timings. No matched
0.1.8/off control establishes an optimization delta. Juan reports pause works;
the submitted run itself has no focus-loss/background event.

The session contains 3,492 surface-rotation messages emitted only inside actual
CreateSwapchain, all observed at unchanged geometry. The first ten rebuild
reason logs say persistent SUBOPTIMAL. Surface transform is ROTATE_90, while
the app intentionally selects IDENTITY to leave output/UI rotation to Android.
CheckSwapchain drains the GPU with WaitIdle before rebuilding. New swapchains
preserve that transform mismatch, so recreating cannot settle these reports.
Existing policy backs off 30 *presenter* iterations, not 30 guest frames.
This is a confirmed repeated rebuild defect; its exact FPS cost is unmeasured.

581 hitch reports: 11 include pipeline creation or nonzero pipeline wait.
Loading/scene entry includes about 223 ms pipeline waiting; later scene setup
includes 83/88/134 ms waits. Most warm racing hitches have zero pipeline work.
No current-session graphics pipeline failures; old dumps in the private archive
do not establish a current failure. No evidence that async causes the steady
35 ms GPU cost. Desktop shader/driver caches and compiler costs are not a control.

## Competing hypotheses and cheapest discriminating tests

1. Persistent presentation rebuilding worsens cadence. Drive actual Android
policy through 10,000 persistent same-size reports; require zero rebuilds,
but require real size/OUT_OF_DATE/resume recovery. On-device confirmation is
stable [surface-health] creation count and comparable warmed frame times.
2. Individual rendering passes, resolves or texture work dominate GPU intervals.
Record each operation separately during ordinary racing; long GPU intervals
with short all-operation spans would implicate uncovered waits/gaps instead.
3. Shader/driver compilation explains intermittent hitches, not all low FPS.
Record queue delay, compile time, cumulative CP wait, async mode, loading wait
and skipped draws. Repeat the same warmed race with the live async toggle off.
An unchanged hitch with zero pipeline activity weakens that attribution.

## Changes and invariants

Android's default policy ignores SUBOPTIMAL as a rebuild trigger. Surface extent
checks, genuine resize, OUT_OF_DATE and native surface-loss/resume recreation
remain. Output orientation/UI are unchanged. SB_ANDROID_SUBOPTIMAL_REBUILD=1
restores the previous policy. Desktop/iOS policy is unchanged.

Per-slot timestamp pools: 256 operation pairs per existing command-buffer slot.
Reset only after the slot's previous fence is retired and results harvested;
read only on the completion thread after its fence signals, without WAIT_BIT.
No image readback, additional submission, Flush or device-idle call is introduced.
One frame after every 120 complex frames (>=1,500 draws), starting after 30;
at most 120 sampled frames. 40,000 CSV rows maximum. Counter wrap uses the
queue's timestampValidBits; unavailable results are -1 with status, never zero.
Overflow is explicit. Query-pool creation failure retains normal rendering.

gpu-passes-*.csv under logs records frame, submission, slot, operation ordinal,
kind, duration, preceding intra-submission gap, dimensions, target/resolve base,
format, attachment masks, draw/depth-only/alpha counts, last shader hashes,
bytes estimate and dropped regions. Submissions spanning frames are grouped by
both frame and submission; submission_gpu_us covers the whole submission and
must not be added repeatedly across rows. Render-depth-only describes attached
targets, not a guaranteed semantic shadow pass. Depth-only draw count also
identifies synthetic solid-caster draws inside mixed-target passes. Last shader
hashes identify the last draw, not an assertion every draw shared one shader.
Render bytes are structural load+store estimates, not measured transactions.

BOTTOM_OF_PIPE markers bound ordered operation intervals. Markers can serialize
overlap and perturb sampled frames; they are not pure shader-cycle counters.
Gaps include uncovered barriers and possible submission/CPU idle time. They are
not assigned blindly to the next operation. performance-*.csv marks sampled
frames per reporting window; GPU completion can cross that window boundary.
SB_GPU_PASS_TIMING=0 disables sampling. Existing presenter AA/scale timestamps
and CP/guest wait counters remain available to cover the rest of the frame.

Shader diagnostics preserve existing worker count, async preference and loading
correctness waits. Up to 512 adopted pipelines log queue/compile/CP-wait costs,
skips, queued/adopted frames and policy. Pending pipelines continue to appear
in existing skip counts; an unadopted pipeline has no final compile row yet.

Collector keeps scheduling/work reduction fixed, snapshots settings/version,
captures both shader modes if requested, then presses Home and waits ten seconds.
It packages logs/thermal/CPU/exit evidence and explicitly checks current-run
pass-output initialization. No ADB connection is made by cloud Work.

## Validation and device acceptance

Host timing bounds, sampling limits, query exhaustion, unavailable results and
8-/64-bit wrap tests pass. Existing swapchain scenarios and mutation checks plus
Android persistent-report/resize/OUT_OF_DATE/inactive/resume tests pass.
Collector bash syntax and a mock-ADB end-to-end run passed both current-output
confirmation and missing-output warning, archive creation and automatic Home.
Depth-only/depth24, ZPD, GPU work/lifecycle, scheduling, whole-vblank and fullscreen
regressions passed. Full build/signing metadata is recorded on completion.
These are host/build checks, not RP6 GPU execution or a measured speed gain.

Next device test: one install, warm same Quick Race, drive two minutes, repeat
with async off if desired, then type done. Return the archive plus FPS/GPU/hitch
observations. Require GPU-pass CSV rows and [surface-health] stable creation
counts before declaring measurement complete. Preserve 0.1.9 as rollback.


## RP6 result received October 16

24 sampled frames / 1,145 operation rows, zero unavailable/overflow. Actual
per-slot timings work. Surface creation count stays one despite >4,400 persistent
reports; repeated reconstruction is fixed. Focus loss then suspension/pause/stop
is captured. Async remained on. Warming hitch reports rarely contain pipeline
work. Aggregate complex-window GPU median remains 33.6 ms; no matched speed delta.
See ANDROID_60FPS_INVESTIGATION.md for pass attribution, compiler probes and a
ranked implementation plan. No additional APK was produced for that investigation.
