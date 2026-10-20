# Android development checkpoint — October 8, 2026

## Project and current milestone

SpeedBreaker One is onenonlygit's personal, AI-assisted Android ARM64 fork of
SpeedBreakerProject/speedbreaker (project(u)). Preserve upstream attribution,
GPL license, renderer/runtime and existing artwork. Android work lives on
`android/rp6-bringup`; main retains upstream code with a fork overview.

**Juan completed a full Quick Race sprint on Retroid Pocket 6 using 0.1.2.**
This is a working-gameplay checkpoint, not a stable or optimized release.

| Area | Latest evidence / limitation |
| --- | --- |
| Graphics | User reports no visibly broken graphics in this race; screenshot shows road, buildings, car, mirror and HUD. Broader accuracy unverified. |
| Quick Race | One complete sprint reported. Repeated races, other tracks/cars and long sessions not verified. |
| Career | Previous build could skip videos and reach an opening cinematic, then had missing world geometry. Latest Career progression not tested. |
| Audio | Music and some effects play. No engine/car audio, selective other sounds. Cause unresolved. |
| Performance | Racing samples around 20.6–30.1 FPS at internal 1280×720. Display swapchain 1920×1080. Not a 1080p render benchmark. |
| Memory | New meminfo: PSS 2,033,238 KiB (~1.94 GiB), RSS 2,139,372 KiB (~2.04 GiB). Track growth across races. |
| Graphics pipelines | No failed links or compatibility-fallback recoveries in either new session. |
| Presentation | Only initial swapchain setup; persistent SUBOPTIMAL accepted. Previous per-frame recreation stopped. |
| ISO | Direct reads of 49-file supported disc layout; picker permission remembered. No extraction or duplicate ISO required. |
| Suspend/resume | Deferred by user; not the present priority. |

## Engineering history

1. Established Android ARM64/SDL/Vulkan toolchain and native game APK, retaining
   SpeedBreaker's runtime/frontend. Added Android file picker and direct disc
   source/mount support rather than requiring extracted game files.
2. First game build reached menus, videos and Quick Race selection. Low-memory
   process kills, sideways presentation and Adreno pipeline rejection blocked racing.
3. `0.1.1-android-quickrace` / `da22647c89536f5b26ce82099fda4170eaff986b`:
   window-oriented identity presentation and immersive fullscreen; Android upload
   ring 64 MiB/four slots; lazy GPU-shadow page population; 32 MiB image blocks;
   one pipeline worker; bounded shader captures and memory reporting. Quick Race
   still killed. Career showed cars but missing roads. See ANDROID_QUICK_RACE.md.
4. `0.1.2-android-race` / `9809202a33be4f823202293612ee8469c984c0f4`:
   stop Android swapchain recreation on SUBOPTIMAL; ordinary vertex draw path
   separate from rectangle expansion; explicit constant initialization/redirect
   checks; pinned glslang SPIR-V optimization and validation; one older-SPIR-V
   fallback on VK_ERROR_UNKNOWN; separate cache keys; per-session failure captures;
   actual translator synthetic regression test. See ANDROID_RACE.md.
5. Device test of 0.1.2: completed sprint, world graphics restored, incomplete
   audio and low frame rate remain. New sessions: `2026-10-08_18-00-14.log`
   (race) and `2026-10-08_18-03-01.log` (second launch/menu). The supplied exit
   report lists older exits up to 17:32, not a new 0.1.2 memory-kill diagnosis.

## What the latest logs support

Racing command-processor busy time is approximately 11–16 ms/frame, with roughly
2,300–3,550 draws, 20–21 render passes and about 21 resolves/frame. GPU completion
waits dominate many logged hitches; steady racing has negligible pipeline waits
and few texture uploads. Rendering/resolve cost and synchronization are the
strongest performance leads. The `GPU busy` log is not a reliable pure shader
execution timing breakdown (`executing` currently reports zero); add/verify
GPU timestamps before asserting a specific GPU shader bottleneck.

Audio opened as six-channel 5.1 float at 48 kHz, including a six-channel device
format on Android. Inspect channel energy and stereo downmix/routing as well as
XMA and engine-specific sound decoding. File-open logs say engine `.abk`/`.gin`
paths are absent as loose files, but also report absent geometry/music paths
that visibly/audibly work via packed archives. These messages alone do not prove
missing assets or a direct-ISO bug. Trace packed ZDIR/ZZDATA lookup and actual
read/decode results before changing the disc loader. Preserve direct ISO access.

## Next work, in order

1. Restore engine/car audio: trace engine banks/GIN decoding and mixer inputs;
   verify six-channel-to-stereo output. Compare music/effects paths and avoid
   treating every loose-file miss as fatal. Josie video audio also previously absent.
2. Reduce rendering cost: profile pass/resolve work, oversized EDRAM targets,
   shader memory accesses and completion waits. Keep current graphics correctness.
   Test changes independently against this known working APK.
3. Consecutive-race stability: memory snapshots at menu/load/driving, race finish,
   next race. Determine retained caches versus growth/leaks. One race is not soak testing.
4. Then broader car/track/Career coverage and performance tuning. No unsupported
   promise of 1080p60 or a power target; suspend/resume remains deferred.

## Reproduce and continue

Start from `android/rp6-bringup`; read this checkpoint, ANDROID_RACE.md and
ANDROID_QUICK_RACE.md. Latest tested APK and checksum are committed under
`builds/android/0.1.2-android-race/`. Do not replace that APK with an untested build
under the same name/version. Exact binary metadata and device instructions live
beside it. The APK was built before this documentation commit; its source commit
is the 9809202 commit above.

Use existing Android build/dependency scripts. NDK r28c (28.2.13676358), Java 17,
Gradle 8.12, Android SDK/build tools 35, pinned SDL 3.2.24 and glslang 15.1.0 with
its known_good SPIRV-Tools/Header revisions. Release test command:

```sh
cd android
gradle --no-daemon -PsbDiagnostics=OFF -PsbBringup=ON :app:assembleRelease --max-workers=4
```

`sbBringup=ON` keeps the optimized release debuggable for `adb run-as` log capture;
it is not a debug/unoptimized renderer. Generated PPC source, supported XEX and
tool checkouts are intentionally excluded from Git; regenerate locally using
the documented scripts and the user's own game. Do not publish raw shaders,
user device logs, disc images or extracted assets. Diagnostics were analyzed
locally; this document records the relevant sanitized results.

The supported XEX SHA-256 is
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
APK package: `com.onenonlygit.speedbreakerone`, version code 3.
Keep the existing private signing key/certificate for future install-over builds;
never commit the key or signing passwords. Certificate fingerprint and matching
native Build IDs are in the APK's `build.json`.

Offline validation passed: original captures validate; 72 revised cross-stage
pairs / 144 modules (three primitive modes, two targets) compile/validate;
eight synthetic modules emitted by the actual translator compile/validate;
synthetic translator test, optimized signed ARM64 build, signature and matching
symbol checks. The APK exports 56,165 translated guest functions.

Device collection must run locally: cloud cannot connect to the RP6 LAN.
Last wireless ADB endpoint was `10.0.0.109:35469`; Android can change the port.
Capture logcat before launch and exit-info before relaunch after failure.

## Follow-up phase: 0.1.3 audio/measurement (not yet device tested)

After the 0.1.2 checkpoint was saved, user authorized continuing with remaining
usage. Android playback now requests stereo while retaining six-channel guest
mixer input, so SDL downmixes before output. Functional dummy-device test against
pinned SDL passes all six individual channels. Aggregate channel RMS/non-finite
counts, XMA stats and supported GPU timestamps are enabled for Android. The
optimized signed ARM64 APK builds; no engine-audio recovery or performance gain
has been established on RP6. See ANDROID_AUDIO.md. 0.1.2 remains the last known
working device-tested APK; do not relabel it as 0.1.3 or overwrite its release.

## 0.1.4: Android viewport-area trial (October 8, 2026)

Baseline: `0acaa1b`, 0.1.3 audio-test. User reports most audio working and
21–39 FPS racing at 720p, 60 FPS menu. This supersedes the earlier untested
0.1.3 status; no new logs were reanalyzed for this change.

One optimization: Android defaults the existing `NFSMW_UNTILED_AREA` path on.
Untiled draws and dynamic-rendering areas use the clipped guest viewport rather
than the entire oversized EDRAM target. Existing checks retain whole targets
for disabled clipping, invalid viewport transforms, incompatible area unions,
pass expansion and resolves reading beyond the cut region. Target allocations,
resolve shaders, pass merge policy, barriers, submission slots, audio and ISO
loading are unchanged. Environment override `NFSMW_UNTILED_AREA=0` restores the
baseline policy. Desktop defaults remain unchanged.

Evidence: racing has ~20–21 passes/resolves and completion waits, with little
steady pipeline/upload activity. Code shows Android uses full EDRAM areas,
e.g. 1280x1024 or larger for a 1280x720 view, and full-size cube-face areas.
Reducing area can reduce overdraw and tile attachment traffic. Upstream reports
benefits on Apple but a small regression on Linux Turnip; stock RP6 Adreno is
unmeasured. This is a bounded trial, not a performance claim. Existing resolve
fallbacks do not constitute proof of graphics equivalence across every scene.

Synchronization review: completion-thread fence waits protect submission/upload
slot reuse and guest memory access. Queue-idle calls found in presenter setup,
resize/teardown are not evidence of a per-draw stall. Full inter-pass barriers
remain conservative. Draw caching and pass merging already exist; changing these
at the same time would obscure attribution. No new profiling query pools: retain
0.1.3 submission GPU timestamps and enable existing `[untiled-area]` aggregates
once per 120 frames (override `NFSMW_LOG_UNTILED_AREA=0` disables them). Reported
MB loaded+stored is a model, not a hardware bandwidth counter.

### RP6 comparison

Install 0.1.4 over 0.1.3 without clearing data. Use internal 1x (1280x720), the
same car/Quick Race/track, device performance mode, brightness and thermal state.
Warm caches with one race; measure the next identical 2-minute race. Record FPS
range/typical FPS, frame-time spikes and watts if available. Repeat the baseline
under equivalent conditions using preserved 0.1.3 (downgrade may require adb
`install -r -d` because version code is lower; keep data).

Collect full logcat and app session logs, including `[perf]` GPU executing,
CPU busy and completion/slot waits, and `[untiled-area]` Mpx/MB, pass restarts
and resolves-read-past. Check roads, car, mirror, reflections, shadows and HUD,
then verify engine audio/music and start a second race. FPS improvement alone
without comparable warm/thermal conditions is insufficient. Regression: retain
0.1.3 and revert only the Android viewport-area default.

### Offline validation and artifacts

Optimized debuggable ARM64 release build passed (existing NDK/Gradle setup).
APK v2 signature verified against retained certificate
`29ebbb37c610217ff00755e3731df6a1db46eda4cec45555157704ab545902bf`;
package unchanged, version code 5. Native libmain Build ID differs from 0.1.3.
Existing host disc-mount, SDL stereo-downmix and write-watch tests (subpages off
and on) passed; git diff --check passed. These do not exercise Vulkan rendering
on Adreno. No RP6 access or graphics/performance validation was performed.

Checksums and native Build IDs are in builds/android/0.1.3-android-audio/
(preserved baseline) and builds/android/0.1.4-android-area/ (new trial).
Both APKs are preserved in the build workspace; 0.1.4 is delivered as a download.
The authenticated source commit does not include APK binaries; shell Git push
was unavailable because this workspace has no GitHub shell credentials.

## 0.1.5 visual diagnostic source checkpoint — October 9, 2026

Source investigation found D24FS8 float-versus-UNORM conversion inconsistency
and missing fragment depth export (`writesDepth` / `o_depth` never reaches
`gl_FragDepth`). Their usage in affected racing shaders is not established;
no speculative lighting fix applied. Added bounded three-frame render-state
capture using existing pass profiler, depth/guest-word/texture/front snapshots,
draw and occlusion state, plus local-only unattended collection script.

Version metadata is staged as 0.1.5 code 6, same package. **No APK built:** actual
PPC sources/XEX and Android SDK/NDK dependencies are absent in the surviving
workspace, and Gradle download is network-blocked. Private signing backup opens
and matches the retained certificate. 0.1.4 artifact remains unchanged. Host
syntax checks with compile-only compatibility fixtures, capture-budget test,
existing ZPD tests, bash syntax and diff checks pass; Android/GPU validation is
outstanding. Read ANDROID_VISUAL_FIDELITY.md for exact limitations and build steps.

## 0.1.5 signed APK completed — October 9, 2026

The supplied supported USA XEX regenerated actual PPC code. Recovered SDK/NDK,
JDK, Gradle and pinned dependencies; proxy bootstrap resolves dynamically from
the environment. Patched FFmpeg extraction to avoid archive ownership changes.
Full optimized ARM64 release passed (sbDiagnostics=OFF, sbBringup=ON). APK v2
certificate matches the retained signing identity; same package, code 6.
56,165 unique translated functions and new capture code verified in libmain.so.
Build ID d749b95f479de813e81d7274c7af60a67f3915ac. APK SHA256
1d0948eb2e745e410139513e0a0700788b6330888784e854190199fe760ed490.

Capture now rotates one depth resolve across three selected frames, records
matching end-of-frame sampled R32F data, preserves pass metadata if timestamps
are unavailable, and reports budget-limited output. Fixed collector remote-shell
quoting. Capture-limit, actual shader translator, eight optimized/validated
SPIR-V modules, ZPD, ISO reader, PNG/ZIP and syntax checks pass. These supersede
the previous build blocker. No RP6/ADB/LAN access or visual fix performed.
0.1.4 is preserved. Metadata is under builds/android/0.1.5-android-visual-diagnostic/.

## 0.1.6 upstream lighting and methodology — October 10, 2026

Selective v0.1.1 integration (`05838d6`): shared GLSL depth pack clamps to
24 bits before shifting. Exhaustive separate/FMA boundary regression and actual
resolve integration passed. Published lighting commit `237b0d7`; methodology,
roadmap and version commit `2efca3b` (the APK's clean source identity).
`AGENTS.md` requires ENGINEERING_METHODOLOGY.md for future work. See
ANDROID_UPSTREAM_016.md for other changes deferred and competing flicker hypotheses.
No extra performance policy/quality change; no measured gain claimed.

Optimized full ARM64 release passed, sbDiagnostics=OFF, sbBringup=ON.
Package retained, name 0.1.6, code 7; native version also 0.1.6.
Certificate matches preserved 0.1.4/0.1.5. APK SHA256
`d8a4b30c1340f3e1091cfdc3594aba309b53341c1544e7e32a627e13793d918c`;
libmain Build ID `1b1c1a2d32394a1eb88f39f5d229b06cfa17b57d` matches
packaged/unstripped binaries. All 56,165 translated functions present.
Metadata, checksum, release notes and minimal test are in builds/android/0.1.6-android-lighting/.
No XEX, generated PPC source, signing secrets, raw shaders or device logs published.

Validation: depth exhaustive/round-trip/text/integration test; eight actual resolve
variants and eight translator modules compiled optimized for Vulkan 1.1/1.2 and
SPIR-V validated; capture, ZPD, ISO concurrent/lifetime/bounds, atomics, block ranges,
frame policy, whole-vblank and write-watch modes 0/1 pass. Write-watch requires
Clang (GCC rejects existing PPC debugtrap/anonymous aggregate syntax); host Clang
from NDK with Linux sysroot succeeds. Existing SDL routing was retained/compiled;
no new host audio test or device audio validation was run. Native SDK/NDK/JDK/Gradle
were restored from official sources; Gradle archive hash matched pinned checksum.
No RP6, ADB/LAN, Career or desktop gameplay execution occurred.

Most valuable next test: two-minute same-scene 720p Quick Race, checking restored
sunlight and stable shadows; evaluate warmed `[perf]`/`[hitch]` only after the three
bounded captures finish. Check audio/mirror/HUD and start a second race. Source
review alone cannot identify the RP6 critical path. Return one private archive.

## 0.1.7 isolated upstream solid-shadow test — October 12, 2026

Baseline 0.1.6 (`95533b7`); upstream v0.1.3 (`b49fc32`). Ported the depth-only
shader semantics fix: solid casters no longer run the stale alpha-killing tree
pixel shader; depth-only alpha testing is suppressed and guest shader binding
is restored afterwards. Import helper/test verbatim, retain 0.1.6 depth24 fix.
No unrelated upstream timing/trails/tuning or renderer quality policy changed.
Android/native version 0.1.7, code 8, package and retained signing key preserved.
Collector version check accepts capture-enabled codes >=6. See ANDROID_UPSTREAM_017.md
for source trace, regression tests, competing hypotheses, rollback and single RP6 test.

0.1.7 build completed: optimized signed ARM64, clean source `02a9f37`, code 8,
package/certificate unchanged. APK v2 verifies. All 56,165 unique guest functions
and the new shader path are present. Native Build ID
`8e2b9de560f28dd1ff8c82c773351218156265db`; APK SHA256
`54ad66b3d474e07f18e15416ce4c6e919d309408931bb10fc8ac646a14daf664`.
Artifact is delivered directly; verification and test instructions live under
builds/android/0.1.7-android-shadows/. Preserved 0.1.6 checksum/signature match.
Depth-only, exhaustive depth24, optimized SPIR-V (Vulkan 1.1/1.2), ZPD, ISO reader
and capture-limit tests pass. No RP6/ADB/LAN access; visual result remains untested.


## 0.1.8 signed performance trial + upstream v0.1.4 — October 15, 2026

Full upstream v0.1.4 merge, not only the earlier shadow/timing backports.
See ANDROID_UPSTREAM_018.md for conflict decisions, invariants and tests, and
ANDROID_PERFORMANCE_018.md for the reversible GPU scheduling trial. Gamma /
brightness, nitrous trails, tuning jump tables, game timing and shared renderer /
presenter updates are included. Regenerated game sources from verified USA XEX.
Android ISO/stereo/lifecycle/shader/capture paths and accepted shadows retained.

Clean compiled source 3bea7fcf84d8b44363249ab8c999022d59194f51; Android/native
0.1.8 code 9, same package and certificate. Full optimized ARM64 link, all 56,146
generated mapping symbols, 26 host suites, eight optimized/validated SPIR-V
modules, package/library/ZIP/signature checks passed. SDK Build Tools 35/aapt2
used via packaging overrides; no fixture/game-free runtime is packaged.
APK SHA256 d8f15a7863e16f43f50f5205bafed580b0d1e1d61f9af8a8579c600800a83526.
Build ID 0289cb8044777d7e9de1556db25cd9a8bd05dd42.
Preserved 0.1.7 checksum is unchanged. Build record under
builds/android/0.1.8-android-performance/. No RP6 execution or speed gain verified.

Next test: one install, same 720p / Frame Rate 60 Quick Race, warm a minute,
then GPU Scheduling Compare for four minutes, second race and background/resume.
Return one local capture_performance.sh archive and observed FPS/power.
Conservative reverts scheduling only; 0.1.7 is the full rollback.


## 0.1.9 device result and 0.1.10 measurement correction — October 16, 2026

0.1.9 optimized signed APK completed from clean 7cc2e0e, code 10, package and
certificate unchanged. APK SHA256 b684df46c5f14f0e5ce718a327e7162faae9e12a8849d45d769847dce333c0fe;
native Build ID 10a055a301dc4135cf892c233421b8c72cc53132. All 56,146 generated
mapping symbols linked; host regressions, 32 optimized SPIR-V variants and APK
integrity/signing passed. No proven speed claim accompanied the build.

Juan reports performance still poor and confirms pausing works. His private
Oct 16 11:22 capture confirms work reduction active, 720p internal scale,
~35 ms median completed GPU time and ~28 FPS in complex windows. The prior
aggregate-only capture missed individual operation timing; that was insufficient
for the planned diagnosis. No equivalent baseline/off control was captured.

A second source/log review found 3,492 actual swapchain creations from persistent
rotation-related SUBOPTIMAL, each policy rebuild draining the GPU. 0.1.10 removes
that trigger on Android while preserving resize/OUT_OF_DATE/surface recovery.
It adds bounded automatic per-slot, per-pass/resolve/transfer timestamps, explicit
sample/availability/overflow metadata, shader policy/compile/wait/skip evidence,
and cumulative surface-health counters. No readbacks, extra GPU waits or quality
reduction. Existing asynchronous-shader preference and loading waits remain.
Collector verifies current pass output, snapshots settings/version/thermal state,
and automatically presses Home for a ten-second background check after done.
See ANDROID_PERFORMANCE_0110.md for invariants, limits and falsification tests.


### 0.1.10 signed APK completed

Clean compiled source 56d3fb6, Android/native 0.1.10 code 11. Full optimized ARM64
runtime, all 56,736 mapping entries (56,146 sub_* game functions plus register
helpers), new diagnostic markers and packaged library byte identity verified.
ZIP, alignment, version/ABI/debug access and retained v2 certificate pass.
APK SHA256 f2e359ccf87c6f25216ace118e3905386d1d1bbdd65a02c5e4ce37b366d8371e;
native Build ID 540ce9a82e58fcc24d1a932c2020ea336276174b. Build record under
builds/android/0.1.10-android-gpu-timing/. No RP6 execution or gain verified.


## 0.1.10 RP6 measurements and 60 FPS investigation — October 16

Device capture validates pass sampling (24 frames, 1,145 rows, no missing or
dropped queries), stable single swapchain creation and Home suspension. Warmed
GPU cost remains ~33.6 ms per aggregate sample; no controlled old/new delta.
Color rendering ~25.9 ms, depth-only ~5.0 ms, resolves ~3.5 ms per sampled frame.
Detailed fixed-size pass analysis and native reference audit point to vertex
fetch/input, shader predicate control flow, safe depth-only stage elimination
and guarded overdraw work. CPU hitch-subset core times also approach the budget.

Confirmed an offline compiler issue: nine separate same-predicate sample blocks
remain nine branches after our optimizer; one shared block gives one branch,
retaining all nine samples. Eight Vulkan 1.1/1.2 probe modules compile/validate.
Reproduction script scripts/analysis/probe_predicate_codegen.py is synthetic,
not a hot game shader, Adreno cycle test or FPS claim. Full implementation plan,
reference/settings audit, safeguards and acceptance criteria are in
ANDROID_60FPS_INVESTIGATION.md. No production renderer change or new APK from
this investigation; 0.1.10 remains the retained device baseline.

## Android 0.1.11 implementation — shader/depth trials and Turnip selector

Implemented the first authorized renderer changes on android/rp6-bringup:
generation-safe instruction predicate sharing within EXEC and conservative
no-color fragment-stage elimination, with true position-only vertex outputs.
Independent restart controls retain the old paths. Alpha/kill/depth/memexport
cases retain fragment effects; native raster/depth/stencil and casters stay.

Added an Android launch-screen Turnip ZIP import/selection and System recovery
option, isolated from the :game Vulkan process. Rootless libadrenotools loading
and hidden Volk dispatch cover runtime, ImGui and direct Android surfaces.
Startup probes actual Turnip identity and falls back on loader/probe failure;
actual driver details and requested/eligible depth-stage and geometry/snapshot
counts are included in diagnostics. No custom driver is redistributed.

44 host suites, Java archive boundaries, release Java compilation and 94 optimized
synthetic SPIR-V variants pass. Full ARM64 link succeeds. Device gains/Turnip
compatibility remain untested; hardware vertex-input conversion remains later.
See ANDROID_PERFORMANCE_0111.md for invariants, hypotheses, rollback and next test.

### 0.1.11 signed APK completed

Clean source 53fcdf5, Android/native 0.1.11 code 12. Full optimized
ARM64 runtime and all 56,736 mapping entries verified. Seven packaged libraries
match staging, including four driver hooks; Vulkan dispatch symbols are hidden
and no direct system-libvulkan import remains. Signature/certificate, ZIP,
alignment, manifest/version/ABI/debug access and notices pass.
APK SHA256 b04e0640b2022d7937b031370cf2fb5c5f452fb1d18c840e9f093676402e3045;
native Build ID fce54df293c9c7a1768e34a73e86b465393d7607.
Retained 0.1.10 checksum unchanged. Build record under
builds/android/0.1.11-turnip-performance/. No RP6 execution or gain verified.
