# Android 0.1.11: shader/depth trials and custom Turnip selection

Baseline: compiled 0.1.10 source 56d3fb6, investigation f0eb585, Juan's supplied
Oct 16 RP6 session. Keep that APK and its archive. Existing branch unchanged.
No RP6 execution, driver compatibility or speed gain is established by this work.

## Predicate change

Observed offline defect: repeated same-predicate samples survived optimization
as separate branches. Competing explanations for the device cost remain vertex
fetch, binning/overdraw, material mix and driver scheduling. A neutral warmed
comparison would falsify predicate coalescing as a useful improvement here.

The translator now owns an open instruction guard. Adjacent ALU/vertex/texture
fetches may share it only with the same p0 condition. Every state-changing ALU
(including paired scalar/vector operations) closes it after the instruction.
Unpredicated work, opposite conditions, LOD setup and EXEC endings close it.
All CF edges remain boundaries, including loops/calls and outer CondExecPred;
there is deliberately no cross-EXEC merging. Samples, coordinates, arithmetic,
order and LOD selection are unchanged. Existing unpredicated setTexLOD behavior
is retained, rather than silently changing that separate translator policy.

Rollback: Advanced > Shader Predicate Coalescing OFF, restart. Source content
hashes invalidate the affected SPIR-V cache. No cache deletion is required.
Synthetic streams test opposite/unpredicated work, scalar/vector/paired predicate
writes, mixed fetches, EXEC boundaries, jumps, loops, calls and enclosing guards.
These are emitted-source and compiler tests, not a hardware shader equivalence
proof or an Adreno speed measurement.

## Depth stage change

Input path: effective color write mask + API alpha state + translated PS effects
-> pipeline raster bit 7 -> optional fragment stage -> preserved native raster,
depth/stencil and query state. Eligibility requires zero color writes and no
alpha test, discard, depth export, PS or VS memexport. Sample-mask exports are
not implemented by this translator. No geometry, caster, depth bias or samples
are dropped. A real PS can now qualify, beyond the synthetic depth-only shader.

Position-only variants suppress only interpolator declarations/assignments;
gl_Position and gl_PointSize calculations are identical. Module keys include
position-only and SPIR-V-target bits. When a driver rejects the vertex-only
pipeline, retain geometry and retry with the effect-free fragment variant.
No-color alpha-tested/kill/depth-export shaders keep those operations but omit
color exports even when dormant color attachments remain bound. Partial-color
passes retain their previous output interface.

Rollback: Advanced > Depth Stage Reduction OFF, restart. Existing Work Reduction
still independently controls its older synthetic-only no-fragment path.
Exhaustive eligibility test covers all 65,536 masks and 32 effect combinations.
Device visual checks still must include trees/fences, cars, tunnel roofs,
shadows, mirror, cubemaps, motion blur, HUD, audio and occlusion visibility.

## Driver path and ownership

The launch Activity owns the imported-driver list and selection. Import uses SAF
into a new internal app folder, with flat-name/duplicate/path checks, streamed
size/entry limits and ARM64 little-endian ET_DYN validation. Metadata specifies
libraryName, name and optional minApi. Only completed imports are listed, and
previous drivers remain selectable. No driver binaries are supplied by this repo.

The separate :game process receives an immutable Intent snapshot. Resume keeps
its old loader; explicit Restart removes the old game task and starts fresh,
warning about unsaved progress. Normal game Activity destruction also ends that
native process. Home pauses/stops, preserving the existing suspension behavior.
The selector remains outside a crashing custom-driver process.

Before the first Vulkan call, libadrenotools creates the custom loader. A temporary
instance verifies that it exposes Mesa Turnip; failure or silent OEM fallback
switches to System before application Vulkan objects exist. A crash inside a
vendor library cannot be safely caught; selecting System on the launch screen
is the recovery path. Actual Vulkan driver ID/name/info are logged after device
creation. Compatibility is not inferred from the ZIP name.

Volk dispatch is used by every Android runtime/ImGui Vulkan call. All instance
and device entry points load from the chosen loader before use. Android native
surfaces, including foreground recreation, use the same dispatch rather than
SDL's system-loader surface calls. The loader and hook namespaces remain alive
for process lifetime. Dispatch symbols are hidden to avoid ELF interposition.
System-driver execution goes through the same dispatch. No Android libvulkan
DT_NEEDED import should remain in libmain.so. Native hooks are packaged with
useLegacyPackaging=true, extracted to ApplicationInfo.nativeLibraryDir.
Dependencies, submodule pins and full BSD/MIT notices accompany the source/APK.

## Measurement and acceptance

Automatic bounded pass sampling stays enabled. New CSV fields record requested
no-fragment draws, eligible draws even with rollback OFF, submitted nonindexed
vertices/index elements, first/last shader hashes, snapshot draws/upload ranges
and padded upload bytes. These are counts, not measured shader invocations.
Requested no-fragment counts include driver fragment fallback; fallback results
are logged separately. Shader metadata is bounded to 512 translated shaders per
process, with effect flags, resources and instruction predicate/block counts.
No game shader source blanket dumps, GPU readbacks or extra timing waits added.

The collector launches the selector and includes driver selection before/after,
hook diagnostics, settings, timing files and the established Home check.

First test: select System, keep 720p/60 and existing scheduling/quality, both new
controls ON. Warm the same race for 60 seconds, then drive for two minutes and
start a second race. Compare one renderer control OFF at a time after restart.
Compare a selected Turnip against System with the renderer controls fixed,
matching race, settings, clocks/thermals and warm-up. Keep asynchronous shaders
fixed for those comparisons; separately compare warm async OFF if needed.

Acceptance requires unchanged visuals/gameplay/audio plus lower affected warmed
pass times, complete-frame tails and CPU core times. 60 FPS needs <=16.667 ms
complete frames with variance headroom; the old ~33.6 ms GPU aggregate is not a
promise these changes alone halve cost. Hardware vertex input conversion,
guarded sky deferral and lazy materialization remain separate future work.

## Verification

Host: 44 standalone suites pass (SDL-linked audio and PPC write-watch excluded
from this host harness); Java archive boundary suite passes; release Java compiles.
94 optimized Vulkan 1.1/1.2 synthetic shader modules compile/validate, including
position-only and control-flow variants (some duplicate coverage). Full optimized
ARM64 runtime links with genuine generated PPC, not fixtures. Final signing,
packaged library identity and mapping coverage are recorded with the APK build.
No claim of RP6 execution or 60 FPS follows from these host/build checks.
