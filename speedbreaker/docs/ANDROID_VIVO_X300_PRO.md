# Vivo X300 Pro USA bring-up — October 20, 2026

Primary baseline: SpeedBreaker `3fd1250`, `android/rp6-bringup`.
Local working branch: `android/vivo-x300-pro-usa`. No push or publication.
Reference: the local NFSMW-Mobileplus tree, `main` at `461e424`.

## Source and host identity

The local USA dump was opened read-only. Its root `default.xex` is 3,936,256
bytes and matches the supported SHA-256
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
Only that executable was copied to ignored `private/usa/`. The source ISO's
size and modification time stayed unchanged. No disc-derived code or assets
belong in Git.

Observed host: Apple Silicon, macOS 27.0.1, Apple command-line tools present;
no configured JDK or Android SDK/NDK/CMake/Ninja/ADB at the start. Tool downloads,
SDK/NDK, Gradle caches, APKs and signing material live in ignored directories.
`scripts/android/setup_macos.sh` installs a workspace-local toolchain;
source `scripts/android/macos_env.sh` for subsequent build commands.

## Observations from the actual phone

USB shell reports Vivo V2514 / MT6993, Android 17 API 37, arm64-v8a, **4096-byte
pages**. The system `cmd gpu vkjson` report identifies **Mali-G1-Ultra MC12**,
Arm driver r54p1, Vulkan API integer 4206897 (1.3.305).

- All seven baseline renderer device extensions are listed. Dynamic rendering
  is supported. This enumeration alone does not prove logical-device creation.
- `textureCompressionBC` is false. BC1/2/3/5 optimal-format feature masks are zero.
  **Confirmed capability mismatch:** SpeedBreaker currently requests these
  compressed image formats directly. A texture adaptation is required.
- `vertexPipelineStoresAndAtomics` is false. SpeedBreaker's translated vertex
  shader uses read-only shared/ring storage buffers; its memexport registers
  are local variables. Do not transplant ReXGlue feature requirements here.
- `maxBoundDescriptorSets` is 7. NFSMW-Mobileplus's four-set workaround is not
  justified for this phone or SpeedBreaker's descriptor layout.

Raw reports remain private. The game-free APK must independently test memory
aliases/fault recovery, atomics, SDL surface creation, the required extensions
and logical-device creation. BC checks distinguish advertised capabilities from
actual texture upload/sampling. No gameplay or performance is established yet.

## Architecture assessment before adapting the reference

NFSMW-Mobileplus has a CPU BC1–5 decoder after CPU untile/endian conversion.
SpeedBreaker untiles textures **on the GPU**, from its synchronized shared-memory
shadow into a staging ring, then copies to sampled images. Moving that path to
the CPU would require resolving outstanding GPU writes and changes to ownership
and synchronization. Prefer a bounded compute decoder at the existing untile
boundary, preserving guest block addressing, mip packing, cube/volume layout,
write watches, upload barriers and cache invalidation. The reference decoder
can serve as an independent arithmetic oracle in local tests.

Competing hypotheses for later corruption remain shader translation, synchronization,
depth/resolve formats and driver behavior. BC capability failure does not explain
every future rendering defect. First discriminate with synthetic BC blocks,
edge/tiny mip sizes and actual on-phone uploads, before judging game visuals.

Audio stays on SpeedBreaker's patched FFmpeg XMA decoder and SDL six-channel
input/stereo output path. ReXGlue's AAudio integration is not a compatible direct
replacement. Validate engine, opponent cars, effects, speech and music separately.
Keep the current read-only ISO picker/mount and settings/capture infrastructure.

## Isolated work and acceptance

1. Extend the game-free probe: make page size, dynamic rendering, BC image
   capability queries and baseline logical-device creation visible as separate
   results. No change to game memory/graphics policy. Roll back only the probe
   source to remove this instrumentation.
2. Fix build-host portability independently: choose the NDK Darwin host tools
   on macOS; retain Linux behavior and SHA-256 verification. No device policy change.
3. After the diagnostic baseline, implement and test only the demonstrated BC
   incompatibility. Native BC-capable devices must retain their path; retain a
   force-on switch for cross-device comparison. Do not silently lose alpha,
   normals, mip levels, cube faces, geometry or shadows.
4. Generate only the verified USA executable and build an optimized, locally
   signed APK. Keep its matching unstripped library and provenance privately.
5. Test menu, Quick Race, two minutes driving, finish/second race, audio,
   road/car/mirror/reflections/shadows/HUD, and memory. Capture before relaunch
   after failure. A build or synthetic test is not gameplay validation.

Most valuable next test: complete the probe's corrected dialog/Back check, then
exercise real image uploads and game visuals with the USA APK.

## First on-device diagnostic result and Back follow-up

The locally signed game-free APK built, passed v2 signature/ZIP/16 KiB ELF LOAD
alignment checks and installed for Android user 0. The actual APK reports PASS
for the 4 GiB reservation, 512 MiB shared backing, exact A/C/E aliases, read/write
fault retries, atomics, SDL video/audio/gamepad initialization, Android Vulkan
surface, all seven required extensions, dynamic rendering and logical-device
creation. BC1/2/3/5 format and image queries fail as expected. No controller is
connected; one audio output is enumerated. This does not test audio playback,
write-watch concurrency, game swapchains or gameplay.

The user reported Android Back did not exit after closing the results and
testing Home/return. They specifically clarified this was Back behavior, not
a reported freeze. Available logs show pause/stop/resume; exit history shows
task removal and a SIGKILL, with no captured ANR or native-fatal diagnosis.
Do not label that exit history as proof of a graphics crash or a clean exit.

Source boundary: the probe waits for an SDL AC_BACK key, while pinned SDL uses
legacy Activity.onBackPressed and this game Activity is launched into its own
task. Competing causes are modern Back dispatch, root-task background behavior,
or a message dialog intercepting Back. The cheapest controlled follow-up adds
an explicit probe-only Exit button and Android OnBackInvoked callback, logging
which path ran. A build-generated probe flag keeps game Back policy unchanged.
Retain the first APK privately; install the revised probe over it with the same
local key. Verify summary dismissal, Home/return, Back callback/finish and the
Exit button separately before calling lifecycle behavior validated.

The first Back-only follow-up built and installed, but the user reported that
the results overlay still covered the Exit button. Source review establishes a
separate UI defect: SDL 3.2.24 builds a noncancelable message box with an
unscrollable TextView above its buttons. This report exceeds a landscape phone's
height, making dismissal unreachable. The probe now uses its own scrollable
report with fixed Close/Exit actions and asynchronous UI dispatch. This change
does not replace game dialogs or modify the pinned SDL checkout. The explicit
Activity Back callback remains independently reversible; its necessity is not
established by the original dialog-blocked test.

## BC adaptation evidence

The renderer chooses BC1/2/3/5 decoding independently from actual format support
on Android. It expands blocks to RGBA8 in the existing staging untile compute
shader, after the existing endian conversion. The guest block sizes, addresses,
packed mips and write watches stay intact. Expanded uploads split at block-row
boundaries to fit the unchanged 16 MiB staging slot. BC-capable devices retain
native formats; `NFSMW_BC_DECODE=1` forces the new path for comparison and `0`
restores the previous policy. Rolling back the renderer/header change restores
native-only uploads. No occlusion, geometry or audio work is disabled.

Verification actually run:

- Host golden cases and boundary tests pass under UBSan. An independent local
  oracle using the reference project's CPU decoder matches 1,600,000 synthetic
  texels. Both BC1 transparency and BC2/3 four-color modes are covered.
- The actual untile shader's six native/BC/image and shared-buffer variants,
  plus a standalone arithmetic shader, compile for Vulkan 1.1 and 1.2. All 14
  modules pass independent SPIR-V validation before and after optimization.
- **Actual Vivo system-driver GPU:** arithmetic test checks 65,536 words in one
  dispatch; actual staging-untile test checks 602,624 words across 96 dispatches.
  Both have zero mismatches. The untile cases cover BC1/2/3/5, all four endian
  modes, tiled/linear storage, tiny/odd extents, packed offsets, tile boundaries
  and guard words. Logs and binaries remain private.

These compute tests do not establish buffer-to-image copies, sampled rendering,
3D texture addressing, game synchronization, memory use or frame rate. Competing
causes of future visual failures still include shader translation, layout and
sync defects, depth/resolve formats and driver behavior. Test a full game scene
before claiming graphics correctness or measured performance.

The scrollable probe built, passed APK signature/ZIP/ELF alignment checks and
installed over the earlier probe using the same local key. The user confirms
Close results followed by Android Back exits, and the summary's Exit action
also exits. Treat these as reported UI outcomes. Logs confirm summary dismissal
and an underlying Exit-button request, but do not capture a modern Back callback;
they do not prove that callback is required. Home/resume beyond the previously
observed pause/stop/resume remains untested by this last short check.

USA generation completed locally with the hash-gated script and patched
XenonRecomp. The generated source is ignored and has not been packaged yet.

## Touch controls: architecture decision before implementation

User has no controller and requested basic touch controls after the graphics
baseline. NFSMW-Mobileplus's multitouch layout and SDL button/axis snapshot are
useful references. Its game-context/menu guest-memory hooks and alternate runtime
bridge are not a direct fit. Keep a small Android overlay, thread-safe JNI state,
and a main-thread SDL virtual gamepad so SpeedBreaker's existing deadzones,
XInput conversion, menu blocking/latching and settings chord remain in control.
No SDL calls from Java/UI callbacks. Clear state on pointer up/cancel, focus loss
and pause. Preserve physical controller selection, and keep diagnostics free of
the game bridge. Validate simultaneous steering/gas/buttons and all release paths
before claiming driving without a controller works.

## First optimized full APK

The hash-gated USA code generation produced 56,736 mapping entries. The complete
Android ARM64 RelWithDebInfo release built with NDK r28c and two compiler jobs.
The actual CMake cache has diagnostics OFF and compile-check OFF. APK signature,
ZIP 16 KiB page alignment and native LOAD alignment checks pass. Native symbols
include SDL_main, PPCFuncMappings and more than 50,000 guest function exports;
the matching unstripped library is preserved privately with the same Build ID.
The APK contains no standalone XEX, ISO or generated source files, but is itself
a private game-derived binary. It was installed over the probe with the retained
local key. This is build/install evidence; graphics, audio and gameplay remain
pending the phone test. No controller/touch controls in this baseline.

## October 21: actual visual failures and launcher clarification

The user confirms the intro/title screen appears but reports texture corruption
and jumping artifacts. Captures show horizontal white/colored streaks during
video playback; a supplied screenshot also shows full green/black blocks. These
are rendering failures, not a successful graphics baseline. Independently decoding
10 seconds into the original PSA WMV with host FFmpeg produces a clean frame.
The WMV was extracted from the original ISO opened read-only; ISO size/mtime
were unchanged. This rejects source-video streaks as the explanation, without
identifying the failing renderer boundary.

Actual device logs identify the system Mali r54p1 driver, BC decode mask 0xf,
imported zero-copy guest memory and Overlap GPU Scheduling. Competing hypotheses:
texture layout/upload/sampling, imported-memory visibility/ownership, narrowed
barriers, post-processing and shader translation. Synthetic BC arithmetic/untile
success does not eliminate these. First existing-control comparison: Conservative
GPU Scheduling, restore the baseline fullscreen=false, preserve other settings.
A mistaken settings tap briefly changed fullscreen; it was recorded and restored
before the comparison. Preserve settings and logs privately before each test.

Launcher screenshot is already landscape at 2400x1080. User clarified that
"horizontal" means side-by-side launcher buttons, not an orientation change.
Isolated Java change arranges the driver, Play/Resume and Restart actions in one
row, with scrolling for large text/small windows. Driver ownership, SAF, restart
confirmation and the native renderer are unchanged. Build this UI comparison
with the preserved baseline native libraries to avoid mixing graphics changes.

Conservative scheduling is confirmed by the actual `[schedule] mode 0 barrier 0`
log. Captures still contain thin horizontal streaks, so this does not resolve the
failure. User supplied another title-screen capture with stretched/inverted font
triangles, adding vertex/snapshot data corruption to the hypotheses. Do not claim
that BC decoding explains geometry or uncompressed video textures.

Next isolated trial forces the existing synchronized guest-memory copy path
instead of imported host memory. The standalone BC GPU test used ordinary Vulkan
allocated memory, whereas the full game imported the physical memfd/alias mapping;
this is a material untested difference. Retain conservative scheduling for both
arms and preserve the imported-memory APK. The force-copy line is a temporary
Android diagnostic, not a justified permanent default for all Android devices.
The horizontal-launcher-only APK has all native libraries byte-identical to the
baseline and is privately preserved separately.

The user successfully used the touch controls to enter alias ERIC. Actual input
logs record Start, A, D-pad and trigger states. The later screenshot confirms
the overlay is visible; the initial four-second capture was too early to call it
a visibility defect. No functional overlay-visibility correction was established;
state-change logs were added for subsequent checks.

The apparent "Saving ERIC" hang has a 62,688-byte save and 308-byte content
metadata already on disk. The built-in watchdog captured the render thread in
GetPipeline waiting for the worker, and the worker in Mali's compiler. A process
CPU sample shows the Mali compiler consuming approximately one core for minutes;
this is not evidence of a kernel file-write deadlock. Last uncompleted shader pair:
VS f3cb08faf4432651, PS 48553c3b04eb9b2f. Preserve this pipeline's state/variants for
analysis if simpler existing-control comparisons fail. Android debuggerd refused
a nonfatal backtrace because it requires root; the app's existing watchdog report
provided the required stacks without elevated phone access.

Next cheapest test: keep copy memory and Conservative scheduling, disable only
Shader Predicate Coalescing (existing restart option), restart and load the saved
alias. This is a compiler-path comparison, not proof that the optimization is
incorrect. Do not delete the user's save or disable geometry to bypass the hang.

## October 21: touch visibility and compiler comparisons

Basic Android touch controls use an Activity view, JNI snapshots and a main-thread
SDL virtual gamepad. They feed existing XInput/deadzone/menu blocking code; no
reference-project guest-memory input hooks were ported. Physical gamepads take
priority. Pointer up/cancel, Activity pause and focus loss clear input. Both
activities request sensor landscape, and the SDL orientation callback preserves
either landscape direction. Actual user input created ERIC and navigated menus;
multitouch driving, rotation and physical-controller hotplug remain unverified.

A later capture confirmed absent controls despite a visible native flag and
received input. An isolated Java-only comparison rasterizes the touch view with
Android's software layer, leaving SDL Vulkan rendering unchanged. All packaged
native libraries are byte-identical to the preceding SPIR-V comparison. The user
confirms controls visible before and during loading; a new capture also shows
them. This supports keeping the overlay change, without identifying a general
HWUI/driver defect. Private APK and matching native symbols are preserved.

Disabling Shader Predicate Coalescing did not resolve the loading hang. A second
comparison using the existing SPIR-V 1.3 encoding instead of 1.5 also failed at
the same menu transition. Both captured shader pairs pass independent SPIR-V
validation. The subsequent watchdog still places a pipeline worker in Mali's
compiler and the command processor waiting for the pipeline. Save data remains
preserved. Neither failed comparison justifies permanent global defaults.

The first stalled pixel shader has 18 CF blocks with strictly forward edges,
yet the translator emits a general loop/switch state machine. Competing hypotheses
include compiler handling of that control flow, mixed 3D/cube sampling and the
vertex/pixel interface. A private compiler-only reproduction uses the captured
pair/layout, creates no draw submissions and has an external timeout. A private
forward-only control-flow variant retains all texture operations and compiles
and validates for both SPIR-V targets. Device comparison is pending; no translator
change or rendering-correctness claim follows from offline validation.

The standalone captured SPIR-V 1.3 pair exceeds a 20-second external timeout,
without any game execution or draw submission. Recompiling the original pixel
GLSL with the same host tools also exceeds 20 seconds. Keeping the captured
vertex module and changing only forward CF representation compiles in 0.064 s
(SPIR-V 1.3) and 0.063 s (1.5). This supports a control-flow/compiler interaction;
it does not establish rendering equivalence or speed in gameplay.

A source trial emits ordered guarded CF blocks only when every jump is strictly
forward and no loop/call/return exists. Local do scopes preserve taken-edge
breaks, and conditional END retains the original unconditional termination.
Self/back edges, loops and calls retain byte-identical general dispatch. Enable
only for observed Android vendor 0x13b5, Mali-G1-Ultra MC12, driver 226496512;
NFSMW_FORWARD_CF=0 restores the old path. Source-based shader cache keys separate
the variants. 67 synthetic CF cases and the existing predicate-generation suite
pass; six optimized synthetic modules compile and validate for Vulkan 1.1/1.2.
Full-game device verification remains pending. The trial retains SPIR-V 1.3 so
the new APK changes one compiler variable relative to the preceding APK.

The full forward-CF APK reaches the main menu with ERIC preserved, confirmed by
the user and screen capture. The previously stuck pair compiles in 224 ms,
139 ms and 1 ms for its first variants; subsequent variants also return.
Initial world/menu shader compilation totals roughly 2.5 seconds in this session.
No save deletion or draw omission was used to pass the transition. This establishes
recovery of this loading boundary; Quick Race/gameplay remains pending.

Quick Race now runs on the phone. The first captured race shows the car, road,
mirror and HUD; nine logged samples contain nonzero steering and RT=1 together.
A short running sample reports roughly 60 FPS at the current settings, around
12 ms command-processor work and 14 ms GPU executing time. This is not a thermal
soak, general performance claim or completed-race result. Audio diagnostics show
finite, nonzero channel levels and no device-empty submissions/gaps over 50 ms
in the sampled interval; hearing engine/music/effects still requires user testing.

Cleanup candidate restores the original SPIR-V 1.5 initial target and existing
1.3 error fallback, since changing target alone did not fix the hang. It removes
the temporary pre-link capture hook after preserving exact captures privately.
The memory-copy comparison becomes an automatic default only for the exact
observed Mali-G1-Ultra MC12 driver above. Explicit NFSMW_SHARED_MEMORY=import or
shadow remains respected; other devices retain their prior memory policy. This
keeps the copy path that produced clean title/menu and race captures, without
claiming the underlying imported-memory fault mechanism is fully identified.
Full-game verification of this cleanup candidate is pending; the running
forward-CF SPIR-V 1.3 APK remains privately preserved and installed during racing.

## Editable racing controls and reported material brightness

User confirms racing works, but requests separate left/right driving buttons and
an editor. Android overlay now has two steering buttons mapped to the existing
left-stick X axis; the menu D-pad is independent. Opposite steering buttons cancel,
sliding between them is supported, and release/cancel/focus/pause still clears
input. EDIT opens a drag/resize layout editor with SAVE, RESET and CANCEL; normalized
positions/sizes persist separately from game saves. SAVE completes its small
preferences write before closing, since the SDL Activity can terminate its process.
Device captures verify editor opening, drag, resize and a durable saved layout
surviving process restart. Concurrent user/menu touches confounded the first check;
a controlled check was then performed with the user's hands-off authorization.
Only the steering geometry changed by the check was restored; other customized
positions were preserved. Both APK revisions and matching native symbols are
preserved privately. The Java-only APK reuses byte-identical forward-CF native
libraries, so it does not change the reported reflections.

User reports overly reflective car paint. Captured race image shows the reported
bright material. Competing hypotheses include cube-map/resolve data, texture fetch
signedness/gamma/exponent adjustment, shader arithmetic/constants and post-process
exposure. Source review finds texture-fetch sign/gamma/exponent fields declared
in Xenos structures but not consumed by the current sampling translator; actual
use by the car draws is not yet established. NFSMW-Mobileplus handles these in a
different shader/resource architecture, so its code is a reference rather than a
drop-in fix. No arbitrary reflection multiplier or global brightness reduction
was applied. Obtain actual affected draw fetch/constants before a graphics trial.

## NFSMW-Mobileplus reference port and cheat investigation

See ANDROID_REFERENCE_PORT.md for the reviewed reference identity, saved-layout
preservation, Downloads/thermal logging, optional USA cheat hooks, build/device
checks and remaining tests. Original control geometry remains untouched; fresh
save backups preserve subsequent gameplay rather than restoring older ERIC data.
Reflection-fetch states captured so far use unsigned channels and zero exponent;
render-target metadata also reports zero exponent. No reflection attenuation was
introduced. Black Edition is now explicitly requested after the user's successful
reference test, and is offered as a guarded optional USA bonus-content flag.
