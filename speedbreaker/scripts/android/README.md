# SpeedBreaker One — Android bring-up

Personal, AI-assisted fork. SpeedBreaker's CPU, kernel, Vulkan/Xenos renderer,
XMA decoder and patched XenonRecomp are retained. No ReXGlue or game assets.

## Status: GO WITH RISKS

A game-free Android platform probe passed on the RP6 (4 KiB pages, Adreno 740,
stock Vulkan 1.3.128). Memory aliases, fault recovery, atomics and all seven
checked renderer extensions passed; BC compression was reported supported.
Controller buttons registered; stick axes and actual audio playback remain
unverified. The captured process ended through a force-stop, not a native crash.

The supported user-supplied XEX now generates real PPC sources, and the complete
Android ARM64 runtime links with NDK r28c. The full renderer, game startup,
gameplay and performance are **not validated on-device**. The compile-only
fixture path remains separate from the full build.

The probe tests the real shared-memory backend: a 4 GiB guest reservation,
512 MiB shared backing, bidirectional A/C/E aliases with exact +4 KiB E shift,
mprotect/SIGSEGV retries, atomic CAS/futex wake, SDL startup, Android Vulkan
surface and present queue capability. It logs GPU/driver, renderer extensions,
BC support, controllers and lifecycle. It does not create a game swapchain,
exercise the complete write-watch algorithm, play XMA audio or run the game.

Remaining risks: real concurrent write-watch, actual BC texture operations and
shader/pipeline behavior, surface replacement on resume, RAM pressure, audio
and performance. The upstream renderer maps Xenos
compressed textures directly to BC formats; missing BC support needs a texture
adaptation or compatible driver. No concrete fundamental blocker found yet.

## Build

For the macOS/Vivo bring-up and actual device evidence, see
[ANDROID_VIVO_X300_PRO.md](../../docs/ANDROID_VIVO_X300_PRO.md).
On Apple Silicon, `scripts/android/setup_macos.sh` prepares workspace-local
tools. Source `scripts/android/macos_env.sh`, then use `-PsbCmakeVersion=3.31.6`
and, on a memory-constrained host, `-PsbNativeJobs=2 --max-workers=2` with Gradle.
Keep a local signing key and supply its existing `SB_KEYSTORE`, `SB_KEY_ALIAS`,
`SB_STORE_PASSWORD` and `SB_KEY_PASSWORD` environment variables for install-over
builds. Do not put their values in this repository.

Linux x86-64 host, Java **JDK** 17, CMake 3.22+, Ninja, Clang 18+, make, git,
curl, Python and Android SDK. Target API 33+, arm64-v8a. NDK r28c
(28.2.13676358), SDL 3.2.24, glslang 15.1.0, FFmpeg 7.1.1.

```sh
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/28.2.13676358"
sdkmanager 'ndk;28.2.13676358' 'platforms;android-35' 'build-tools;35.0.0' 'cmake;3.22.1'
scripts/android/prepare_deps.sh --probe-only
cd android
./gradlew -PsbDiagnostics=ON assembleDebug
```

Compile the runtime without game code:

```sh
scripts/android/prepare_recompiler.sh --headers-only
scripts/android/prepare_deps.sh
cmake -S android -B build/android-check -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 \
  -DSB_ANDROID_DIAGNOSTICS=ON -DSB_ANDROID_COMPILE_CHECK=ON
cmake --build build/android-check --target runtime-compile-check -j 6
```

The compile-only target uses arbitrary PPC layout constants and emits **objects
only**, never a linked runtime. Those fixtures never enter an APK.

The intended full build needs your own USA base `default.xex`, 3,936,256 bytes,
SHA-256 `aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
Only this executable is needed at the code-generation boundary; a full ISO is
not needed for building. All game assets are required on-device later.

```sh
scripts/android/prepare_recompiler.sh
scripts/android/generate_ppc.sh /path/to/your/default.xex
scripts/android/prepare_deps.sh
cd android
./gradlew -PsbDiagnostics=OFF assembleDebug
```

The full native build path has compiled and linked with actual PPC sources.
The default full configuration fails loudly with no `ppc/`. Public CI builds only
the probe and runtime compile-check. Never publish game-derived code/objects,
`default.xex`, ISO or extracted assets. Keep upstream's generation model.

## Adaptations

Memory uses memfd_create/ftruncate, with ASharedMemory fallback, then MAP_FIXED
only inside the already-reserved guest range. The first Android target requires
4 KiB host pages and preserves the +4 KiB E alias. Larger pages fail explicitly.
A 4 GiB virtual reservation does not consume 4 GiB of RAM.

NDK r28 libc++ lacks atomic_ref. Android guest uint32 words use compiler atomics
with original sequentially consistent defaults and alias-aware shared futex
wait/notify. Desktop and Apple retain std::atomic_ref. Render-queue fences,
PPC sync/lwsync/eieio, FPCR and all other XenonRecomp patches remain unchanged.
API 33 supports existing backtrace calls. Full write-watch stays enabled.

SDLActivity provides landscape/fullscreen/cutout support. Upstream SDL lifecycle,
gamepad and audio paths remain. Android runtime stderr is mirrored to logcat tag
SpeedBreakerOne. Saves/game/logs live under internal `data/speedbreaker/`, caches
under `cache/speedbreaker/`, Android startup selects a local ISO through the system Files picker and retains
its read permission. The XDVDFS parser duplicates the authorized descriptor;
guest disc reads, file attributes and directory enumeration use a read-only
ISO mount. No second ISO or extracted game folder is created. Saves and shader
caches remain in the app folders. The selected URI is remembered for next launch;
a missing or revoked document returns to the picker. Non-seekable providers are
rejected with a local-storage hint. Legacy imports remain under `imports/`.
The runtime starts with NFSMW_RENDER_SCALE=1 for 720p bring-up. Android crash symbols are read from the loaded libmain.so using its own ASLR
bias, with .dynsym fallback for stripped APK libraries. The original desktop
/proc/self/exe path stays unchanged. Offline symbolication still requires the
matching unstripped library.

## Signing

The delivered APK uses a dedicated, retained private SpeedBreaker One RSA key.
Its backup and credentials are separate from git. Never regenerate it per build.
Export SB_KEYSTORE (absolute JKS path), SB_STORE_PASSWORD, SB_KEY_ALIAS and
SB_KEY_PASSWORD; both debug/release use the key when configured. Diagnostic
release builds are debuggable for report extraction; normal game releases are not.
For an optimized full-runtime device test with adb run-as access, build with
`./gradlew -PsbDiagnostics=OFF -PsbBringup=ON assembleRelease`. The bring-up flag
only enables Android debugging access; it does not select diagnostic game code.

CI secrets: SB_KEYSTORE_BASE64 (same JKS, base64), SB_STORE_PASSWORD,
SB_KEY_ALIAS, SB_KEY_PASSWORD. Until these are provisioned CI produces an
**unsigned release APK**, not a disposable signature. No secrets are committed.

## RP6 platform test

```sh
adb install -r SpeedBreaker-One-arm64-diagnostics.apk
adb logcat -c
adb shell am start -n com.onenonlygit.speedbreakerone/.SpeedBreakerActivity
# Close the summary; move sticks/triggers, press buttons; background/resume once.
# Android Back exits.
adb shell run-as com.onenonlygit.speedbreakerone cat files/android-diagnostics.log > android-diagnostics.log
adb logcat -d -v threadtime > speedbreaker-one-logcat.txt
```

Send both files. If it exits before the report exists, send full logcat plus:
`adb shell dumpsys activity exit-info com.onenonlygit.speedbreakerone`.
This APK cannot test menus, races or FPS. Those tests start after generated code
is integrated, the full runtime links and the USA game is installed.

## Foundational validation

The first full Android link exposed FFmpeg AArch64 table relocations requiring
internal C symbols to use hidden visibility. The Android dependency script now
sets that visibility; the patched static XMA decoder links into libmain.so.

CI builds and runs the actual upstream write-watch suite on the Linux host,
including additional A/C/E alias-fault checks. Both watch-subpage configurations
pass on a 4 KiB host. This validates the algorithm on Linux, not Android's
SELinux, Bionic signal context or the RP6 GPU interaction.

The separate `android-write-watch-test` executable builds for Android ARM64 using
the real Android shared-memory backend and AArch64 ESR read/write fault decoding.
It contains no game code. The test Memory constructor never registers or calls
guest functions. The runtime's compile-fixture image constants are irrelevant to
this memory-algorithm test; it cannot load NFSMW.

For future device validation (not required before supplying the XEX):

```sh
cmake --build build/android-check --target android-write-watch-test
adb push build/android-check/android-write-watch-test /data/local/tmp/
adb shell chmod 755 /data/local/tmp/android-write-watch-test
adb shell /data/local/tmp/android-write-watch-test
```

Vulkan device initialization now reports each missing required extension before
creation. Android also logs sampled-image/transfer support for BC1/BC2/BC3/BC5.
These checks diagnose driver compatibility; a BC decoder fallback is not yet
implemented. The Vulkan surface is rebuilt when returning from the Android document picker
(or another foreground transition). Full gameplay suspend/resume testing remains
deferred; picker return and persistent-document access need device validation.

## Direct ISO validation

`disc_mount_test` constructs a synthetic XDVDFS image and exercises the real
parser through both pathname and descriptor access. It checks case-insensitive
lookup, file sizes, directory enumeration, source descriptor lifetime after
close/unlink, bounds and concurrent independent reads. It runs without game
assets and is included in CI. The full native build and Android APK compile
with the mounted-disc guest I/O path; on-device game startup is still unverified.

## Visual diagnostic collector (0.1.5 signed build)

See `docs/ANDROID_VISUAL_FIDELITY.md`. The optimized ARM64 0.1.5 APK is signed with the retained install-over certificate.
After building and installing it with `sbBringup=ON`, run locally:

```sh
scripts/android/capture_visual_fidelity.sh
```

Start Quick Race; the script waits for a new completed capture or bounded timeout
and packages diagnostics/logcat/exit/memory reports. Never run it in cloud Work.


## 0.1.11 driver selection and renderer trials

The launch screen selects System or imported ARM64 Turnip ZIPs (flat bundle,
meta.json with libraryName/name and optional minApi). Import copies the driver
into internal app storage; no root or storage-wide permission is used. Resume
retains a running game's driver. Restart game applies the current selection and
explicitly warns about unsaved race progress. A separate :game process keeps the
selector accessible after a bad driver crashes. System remains the initial choice.

All Android runtime and ImGui Vulkan entry points use Volk dispatch, initialized
from the chosen libadrenotools/system loader before any Vulkan object. Android
surfaces use that same dispatch, including resume. The custom probe verifies
Mesa Turnip identity; failed loading/probing falls back to System. Native crashes
inside a driver cannot be caught safely: reopen the selector and select System.
Actual driver ID/name/info and fallback reasons appear in [driver] log lines.

Dependencies are source-pinned by prepare_deps.sh. Native libraries MUST be
extracted (useLegacyPackaging=true). If packaging a separate sbNativeDir, include
libmain.so, libSDL3.so, libc++_shared.so, libmain_hook.so, libhook_impl.so,
libfile_redirect_hook.so and libgsl_alloc_hook.so. Never distribute a driver ZIP
or forget its separate license when sharing a driver yourself.

Advanced > Shader Predicate Coalescing and Depth Stage Reduction are independent
restart controls, initially ON on Android. Work Reduction remains independent;
Asynchronous Shaders retains your existing preference. Run capture_performance.sh
for the new eligibility, vertex/index and snapshot counters alongside pass times.
Indexed element counts are submitted indices, not unique vertex invocations;
no_fragment_requested_draws can include the fragment fallback (logged separately).
Hardware vertex input conversion and sky reordering are not included in 0.1.11.
