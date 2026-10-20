# 0.1.8 Android GPU scheduling trial

Baseline: branch checkpoint bde3eeefcf8f6c96fadc3f301182c44acc44a354;
0.1.7 compiled source 02a9f372a5a2305a8cb7b7217719f162607fdc8a.
Juan reports corrected shadows, approximately 30 FPS with low-20s dips at 7–9 W.
This is not a controlled 0.1.8 measurement. Preserve the 0.1.7 APK.

## Evidence and competing hypotheses

Recovered the Oct 9 performance report and Oct 10 private capture. Historical
warm racing GPU execution averaged 34.64 ms; CP busy 12.57 ms. These overlap.
The capture CSV includes repeated pass segments, sometimes 128 draws, and large
EDRAM attachments. Image readbacks/Flush perturb those captured frames; their
pass/gap values do not establish steady scene cost. Restored solid casters also
change the workload versus these earlier builds.

1. GPU serialization/attachment traffic limits throughput (suspected). Test existing
   guarded overlap against full barriers with the same renderer/scene/settings.
   A repeatable unchanged GPU interval weakens this optimization's relevance.
2. Guest/CP starvation or driver scheduling dominates some intervals (plausible).
   Compare GPU completed time with CP-swap interval and existing CP/slot counters.
   Short GPU execution with long wall intervals weakens saturation as sole cause.
3. Simulation cadence causes periodic jumps independently of rendering (confirmed
   upstream issue #9, not independently confirmed as Juan's hitch). Port only
   WholeVblanks and its regression suite from upstream b49fc32. This is a motion
   correction, not a GPU throughput optimization.

## Independently reversible changes

- Timing commit: v0.1.3 WholeVblanks queue slack rule and upstream tests, same Give
  contract; initially isolated from nitrous/FrameClock changes. Subsequently merged upstream
  v0.1.4 in response to Juan's explicit request; see ANDROID_UPSTREAM_018.md.
- GPU scheduling: Android's default trials existing guarded barrier level 2.
  Settings > Advanced > GPU Scheduling: Conservative (0), Overlap (2), Compare (4).
  Compare alternates 480-frame arms (four existing 120-frame reporting windows).
  Explicit NFSMW_PASS_BARRIER overrides retain legacy behavior. Android viewport-area default, slots, ring size and quality settings are retained.
  Upstream changes to fractional scaling, memory placement and presentation now
  apply too: comparison mode isolates scheduling within this combined build,
  not all differences against 0.1.7.
- Disable automatic image readbacks in this performance build using the existing
  NFSMW_VISUAL_CAPTURE switch; code and explicit environment opt-in remain.
- Bounded 600-row CSVs under logs record CP-swap intervals (not actual displayed
  intervals), p50/p95/p99, policy, complex-frame heuristic and completed GPU-work
  aggregates. First/mixed/policy-transition windows are flagged. GPU intervals
  are completed submissions, potentially straddling reporting boundaries.
- Mask GPU timestamp differences by queue timestampValidBits, including wrap.
  Unavailable GPU query timing is -1 in CSV; it is not zero-cost work.

## Dependency audit and invariant

Data path: CP Draw/Resolve -> Vulkan command recording -> queue submission ->
completion/fences -> guest publication and next resource consumers.

Pass-end barriers retain source vertex shader, depth and color-output stages.
Their execution dependency orders prior shader reads before following compute
or transfer writes. Resolve barriers order compute writes before fragment,
compute, depth/color and transfer consumers. The omitted vertex consumers get a
full barrier when resolved guest ranges overlap vertex buffers or shader-fetched
indices. Host indices come from separately guarded upload-ring snapshots;
constants also use that ring. Texture upload paths retain full barriers.
Translated memexport registers are shader-local values, not storage writes.

For Android, vertex texture use after any tracked resolve conservatively forces
that full barrier, including potential mip/alias consumers; exact base matching
alone is insufficient proof. Full barriers continue at frame-front publication,
tracked dependency hits, the existing 48-pass cap, and policy changes. No fence,
write-watch, guest materialization, attachment store or draw is removed.
Policy changes run on the CP thread between frames. Pending recorded work gets
EndPass + FullBarrier before changing policy. No settings are read by the new
policy from static initialization, before settings::Load.

## Interpretation and acceptance

60 FPS requires less than 16.67 ms/frame. This small bounded scheduling experiment
has no credible predicted FPS multiplier; upstream Frame recorded no barrier
benefit. RP6 stock-driver behavior differs and is unmeasured. A neutral result is
possible. Upstream timing tests establish clock behavior, not actual RP6 motion.

Keep 720p, Frame Rate 60 (avoid Auto/30 masking throughput), same race/car,
driver, performance mode, brightness and initial thermal state. Warm 60 seconds,
then Compare for four minutes and start a second race. Discard transition windows,
menus, loads, warm-up, and windows with too few complex frames. Compare matched
repeat runs; alternate-arm scenes still differ. Promote only a repeatable gain
beyond noise with no worsened tails, visual/audio regression or crash. Watch roads,
solid car shadows, tunnels, foliage, mirror, reflections, HUD and engine/music.

If visual faults occur, switch to Conservative, restart and repeat. That reverts
GPU scheduling only; 0.1.7 remains the full prior-release reference. Report time
of fault and whether Conservative fixes it. Return one collector archive and
observed FPS/power ranges. No Career progression or cloud ADB is required.

## Validation status

Host scheduling selection, compare boundaries, timestamp 8-/64-bit wrap,
unavailable counters, bounded interval statistics, upstream queued-frame suite,
existing depth-only/depth24/ZPD regression tests pass. Full Android build and signing verification passed; exact metadata is
recorded under builds/android/0.1.8-android-performance/.
Build validation does not establish device correctness or a performance gain.
