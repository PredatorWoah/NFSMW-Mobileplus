# 0.1.7 isolated solid-shadow test

Baseline: Android 0.1.6, branch head
`95533b71ff15f6abca5210dedc0a6811ba07189f`.
Upstream: SpeedBreaker v0.1.3,
`b49fc321d1e95707ed0ea3700144497fbaca231e` (October 11, 2026).
Source: https://github.com/SpeedBreakerProject/speedbreaker/commit/b49fc321d1e95707ed0ea3700144497fbaca231e

## Observation, invariant and data path

User reports car/world shadows have incorrect coverage and tunnels let sunlight
through on both desktop SpeedBreaker and Android, while Xenia has correct shade.
Upstream v0.1.3 documents the same solid-caster defect and reports comparison
against Xbox 360 footage. That is upstream evidence, not our own RP6 test.

Xenos RB_MODECONTROL depth-only mode must write rasterized depth/stencil without
running the guest pixel shader. Trace: guest EDRAM mode -> Draw's shader choice
-> texture layout/constants and PipelineKey -> fragment variant -> depth attachment
-> existing depth24 resolve -> tiled guest words / sampled shadow texture.

The old Draw path reused the pixel shader left bound by alpha-tested tree draws.
Solid car, wall, barrier and roof vertex shaders do not supply that shader's
texture coordinates; sampling transparent texel (0,0) could discard every caster
pixel. Existing far-plane depth packing remains fixed from 0.1.6.

## Isolated change and rollback

Import upstream `depth_only.h` and `depth_only_test.cpp` verbatim, and the three
renderer splices from v0.1.2 -> v0.1.3: dedicated empty fragment shader/cache identity,
RAII restoration of the bound guest shader after depth-only draws, and alpha-test
suppression when no guest fragment shader runs. Colour+depth tree draws retain
their original shaders/alpha test. No shadow bias, projection, light direction,
filtering, render-area, audio, ISO, settings, or synchronization policy changed.
`NFSMW_DEPTH_ONLY_PS=1` is upstream's developer A/B override to recover the old
shader behavior; the APK uses the corrected default. Restore the preserved
0.1.6 artifact for binary baseline comparisons; no previous APK is replaced.

Package remains com.onenonlygit.speedbreakerone, version 0.1.7, code 8. Both
Android/native version metadata advance. The local collector accepts supported
capture-enabled version codes >=6 instead of insisting on the obsolete 0.1.5 name.

Deferred upstream v0.1.3 changes: queued-loop timing, nitrous trail history and
switch-table/tuning-label repairs. Deferred v0.1.2 changes: fractional resolution,
presentation/platform/updater policy. This build isolates shadow semantics for
an interpretable comparison, rather than claiming a full upstream merge.

## Competing causes and cheapest discriminating tests

* Stale tree shader is supported by the upstream shadow-pass reconstruction and
  regression model. The imported test checks the source path actually selects
  the empty shader before pipeline/shader checks, restores guest state and
  suppresses alpha test. It must reproduce missing casters under the old rule.
* Wrong projection/bias, stale resolved textures or clipping remain possible if
  coverage/direction is still wrong after this fix. Capture map A (world + cars)
  versus B (world) and draw state on the same scene; valid opaque casters would
  weaken the stale-shader explanation for any remaining defect.
* A driver-only explanation is weaker because the user reports the same defect
  on desktop. No Adreno execution is inferred from host tests or compilation.

## Validation and artifact status

Passed before build: exact upstream helper/test equality; imported depth-only
source/model regression; exhaustive existing separate/FMA depth24 boundary and
round-trip integration test; empty fragment shader compiled optimized with NDK
glslc and validated using spirv-val for Vulkan 1.1 and 1.2; collector syntax and
git diff checks. Actual ARM64 build/signature and binary details are recorded
under builds/android/0.1.7-android-shadows/ after successful verification.

Build inputs are the same supported USA XEX (verified SHA256), retained private
signing key, NDK r28c, Java 17 JDK, SDK 35, Gradle 8.12, pinned SDL/glslang/SPIR-V
and patched FFmpeg versions. No game assets, generated PPC sources, credentials
or personal logs may be committed. No ADB, LAN or desktop game execution in Work.

## Single most useful RP6 test

Install over 0.1.6 without clearing data. Use the same 720p Quick Race, car and
track. Check that the car casts a solid shadow and an available tunnel/underpass
shades both road and car; check shadow direction/stability while turning. After
warm-up and bounded captures, check mirror/HUD and engine/music, then begin a
second race if convenient. Do not benchmark capture frames or enter Career.
Run scripts/android/capture_visual_fidelity.sh locally to collect one private
archive if a defect remains. No hardware improvement is claimed before this test.

## Completed build

Optimized full ARM64 release succeeded (sbDiagnostics=OFF, sbBringup=ON), clean
source `02a9f372a5a2305a8cb7b7217719f162607fdc8a`. Packaged AArch64 libmain.so
contains all 56,165 unique translated guest functions, visual diagnostics and
the depth-only fix. Native/app version 0.1.7, version code 8, package unchanged.
APK v2 signature verifies and matches the retained 0.1.6 certificate exactly.
Build ID: `8e2b9de560f28dd1ff8c82c773351218156265db`.
APK SHA256: `54ad66b3d474e07f18e15416ce4c6e919d309408931bb10fc8ac646a14daf664`.
Size: 20,786,171 bytes. The reconstructed preserved 0.1.6 APK checksum and
signature match its published build record; its files were not modified.
Additional existing ZPD, ISO concurrency/lifetime/bounds and bounded-capture
checks pass. No Adreno graphics, installation or audio execution was performed.
