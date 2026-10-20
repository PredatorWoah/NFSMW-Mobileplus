# Android 0.1.8 upstream integration

User explicitly requested recent upstream updates alongside the pending signed
performance APK. Fetched upstream main on October 15, 2026; latest is v0.1.4,
`d4c206ab98d4bf01906ec81e6534a1efdcdd29ee`. Merge rather than selected backports:
upstream ancestry is retained so future updates have a current merge base.
Android performance preparation is `eb2f3cc`; accepted shadow baseline is 0.1.7.

## Scope and data paths

Includes upstream gamma ramp programming -> display tone -> presenter shader;
brightness settings; corrected nitrous history -> guest matrix hook; corrected
switch tables -> regenerated PPC; queued/low-rate vblank accounting; fractional
resolution and scaled resolve layout; pipeline cache validation and memory
placement; surface-size policy, fullscreen/UI/input and platform fixes.
Mac/iOS branches remain conditional and do not establish Android speed gains.

Conflicts resolved by retaining Android package/native version 0.1.8 (code 9),
Android source filters, ISO install path and stereo routing, shader compatibility,
64 MiB ring/four slots, guarded viewport default, scheduling/capture diagnostics,
and foreground surface recreation. Upstream swapchain policy replaces immediate
resize rebuilds; Android surface loss still recreates the SDL Vulkan surface
before checking the swapchain. The policy checks actual extent after suboptimal,
avoiding rebuilding an identical swapchain each frame. QR runtime source is
included; official desktop updater row is hidden on Android because it cannot
provide this fork's APK. No saves/settings/ISO are erased.

## Invariants and competing risks

- Gamma changes the game picture only; UI remains untouched. Off + brightness 0
  preserves prior picture. A brighter image is not missing shadows. If washed
  out, reset the game's own brightness slider and compare gamma off.
- Solid depth casters, depth24 saturation, ISO lifetime and stereo routing must
  survive. Host depth/shadow tests discriminate a merge error from Adreno-only
  faults; the same race checks the latter.
- Scheduling might reduce serialization, or it might add no gain on Adreno.
  Compare mode alternates policies within this same merged runtime; differences
  from 0.1.7 alone cannot establish scheduling's contribution.
- New memory placement/scaling/presentation could alter GPU cost or lifecycle.
  Allocation logs, same 720p setting and background/resume test discriminate
  those from the barrier hypothesis. Conservative rolls back scheduling only;
  install the preserved 0.1.7 to revert the full runtime.
- Recompiled tuning tables must be generated from current config, not stale
  output. Generation completed from the verified supported USA XEX.

## Validation record

Passed 17 host regression suites: queued and low-rate vblanks, nitrous trails,
display tone and renderer/settings integration, rational render scale and source
integration, depth-only casters, exhaustive depth24 boundaries and source
integration, Android schedule/timestamp/statistics, swapchain policy, front-end
matrix, pipeline cache identity, memory placement, switch-table bounds/tuning,
fullscreen, present scheduling, cursor policy, quit flow and GPU selection.
Full optimized ARM64 linking and signed packaging passed. The APK certificate
matches 0.1.7; package, version code 9, library equality and ZIP integrity passed.
All 56,146 generated mappings are present in the runtime (upstream config changes
reduce the old count). Eight synthetic SPIR-V modules compile, optimize and
validate for Vulkan 1.1/1.2. Final build metadata is under
builds/android/0.1.8-android-performance/.
The settings round trip and eight additional host suites also passed (26 total).
No device execution or FPS improvement has been established.

## Device acceptance

One install over current app without clearing data. Same Quick Race/car/track,
720p, Frame Rate 60, same driver/power settings. Warm one minute, then use
Advanced > GPU Scheduling > Compare for four minutes. Check solid shadows and
tunnels, road/foliage/mirror/reflections, gamma/brightness, engine/music, nitrous
trail, tuning labels, second race and brief background/resume. Report FPS/power
ranges and one capture_performance.sh archive. No Career progression required.
