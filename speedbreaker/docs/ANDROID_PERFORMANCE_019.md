# Android 0.1.9: native-renderer techniques in the existing runtime

## Baseline and observations

Compiled 0.1.8 source 3bea7fc, branch checkpoint a280635. Juan reports about 35 FPS
on RP6. He reports v0.4.2 at locked 60 FPS with all shadows, Xbox 360 quality
filtering and motion blur enabled. SpeedBreaker reports 99% GPU utilization in
his overlay versus 50-55% in that port; clocks/temperature and frame timing have
not been controlled. This is user evidence, not an independently captured benchmark.
He also reports system sluggishness when leaving SpeedBreaker in the background.

References examined: victorgbd v0.4.2 tag 758fb43 and checksum-matching USA APK;
its pinned native implementation codepdbh/nfsmw-android at
6df1501974d0db84893f5a3ca465b616414f6961. Keep the current SpeedBreaker runtime,
branch, package and signing identity. Preserve 0.1.8. No quality shortcuts.

## Competing hypotheses and discriminating tests

1. Excess GPU shading/conversion dominates (supported by historical GPU intervals
   and user's utilization observations). Test guarded early depth, texture
   reciprocals and word-order resolves, with matched quality and frame time.
   Equal warmed GPU times would weaken their relevance to this gap.
2. Submission/CPU starvation or power/clock policy accounts for part of the gap.
   Utilization alone cannot distinguish this. Compare execution intervals, CP
   time, power and scene, without changing affinity or frame policy in this build.
3. Activity/focus event ordering or queued CP work keeps the app active after
   Home (source-supported risk, not yet the proven cause of the reported lag).
   Audit suspend/resume logs and background CPU after Home, notification shade,
   screen lock and returning to the app. No Vulkan submissions while suspended.

## Implemented paths and invariants

### Early rejection / solid depth

Adapt the native renderer's read-only early-depth rule. Require attached depth,
Z-test enabled, Z-write disabled, stencil disabled, no shader depth export, no
memory export and no open occlusion bracket. Pixel variants and pipeline keys
carry the early flag, including legacy SPIR-V and failure dumps. Only hidden
fragments are rejected earlier; color/alpha behavior, geometry and shadow
casters are retained. Opaque alpha-tested vegetation that writes Z stays late.
Synthetic solid-caster shaders can omit the fragment stage entirely with zero
color writes; drivers refusing that pipeline retry the existing empty shader.

### Actual sampled-image reciprocals

Texture views register actual host width/height, including scaled targets; a
32-entry std140 tail stores reciprocals. Changed extents dirty constants even
without a guest register write; destroyed views remove their entries. Placeholder
views are 1x1. Offset sampling uses those reciprocals; unnormalized 2D coordinates
continue using the guest fetch size. The fallback shader keeps textureSize.
Floating reciprocal multiplication may differ from division by float rounding;
no filtering mode, shadow sample count or pixel footprint is intentionally reduced.

### Specialized 1x word-order resolves

For 32-bit, source scale exactly 1x, no scaled-memory writes: group 32x2 pixels
in tiled word order using the existing 8x8 invocation count. Eliminate scale
loops and scaled-pool branches at compile time. Keep exact origin, clamp, format
packing, depth24 saturation, endianness, guest stores, direct-texture stores and
write-watch sequences. Other formats/scales use their previous paths. The new
variant is prewarmed. A simple aligned model touches two rather than four 128-byte
sectors per group; that is not an Adreno transaction measurement or speed claim.

### Background execution

Android event watch pauses on window focus loss as well as Activity background.
Atomic independent focus/background reasons prevent a focus gain from resuming a
still-background Activity. Duplicate callbacks are idempotent. The CP waits at
the top of each batch, including with queued commands, rather than only when idle.
Retain existing guest-clock, audio and GPU-drain suspension and surface recovery.

## Rollback and next test

Advanced > GPU Work Reduction is on for Android, off by default elsewhere;
changing it requires restart. Off restores prior shader/depth/resolve paths;
background corrections remain. Developer overrides NFSMW_EARLY_DEPTH=0,
NFSMW_TEXTURE_RECIPROCAL=0 and NFSMW_RESOLVE_WORD_ORDER=0 isolate each technique.
Keep GPU Scheduling fixed; do not mix its Compare arms into this comparison.

Install once, same 720p Quick Race/car/driver/power mode. Warm a minute, drive
two minutes, start another race, Home for ten seconds, then return. Check exact
car/tunnel/foliage shadows, mirror, reflections, motion blur, HUD and audio.
Optional: disable GPU Work Reduction, restart and repeat. Return the collector
archive and observed FPS/GPU/power. No Career progression or cloud ADB required.

## Validation status

Safety-boundary tests pass for early-depth conditions, partial-group resolve
coverage and both lifecycle event orders/duplicates. Real translated offset-fetch
and alpha-test pixel shaders plus original/new resolve variants compile, optimize
and validate for Vulkan 1.1/1.2 (32 modules). Existing shadow/depth/occlusion,
clock, scheduling, capture and tone regressions and full ARM64 link have passed. A fresh-process settings test confirms the
GPU Work Reduction switch is persisted and applied after restart. No RP6 FPS gain or background recovery is established before testing.
