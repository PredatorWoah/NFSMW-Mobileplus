# RP6 Quick Race follow-up

The first direct-ISO APK reached menus and Quick Race selection on the RP6.
Android's exit report identifies the first test's termination as LOW_MEMORY;
logcat records an lmkd kill while the process held roughly 1 GiB resident plus
686 MiB of swap. This is evidence for memory pressure during loading, not a
diagnosed native exception or proof that the race loader itself is incorrect.
The second launch reopened the remembered ISO and rendered a sideways title
screen. Adreno 740 also rejected several graphics pipelines with result -13.

This follow-up makes these changes:

- Android presentation requests the supported identity transform because the
  final pass and ImGui do not pre-rotate their content. The previous
  `currentTransform` selection claimed otherwise. Surface transform, extent,
  and window size are logged on swapchain creation. Native pre-rotation is a
  later optimization; see the [Khronos surface rotation sample](https://docs.vulkan.org/samples/latest/samples/performance/surface_rotation/README.html).
- Modern Android system-bar hiding restores the full landscape viewport,
  including after returning from the document picker.
- The upload ring shrinks from 192 MiB to 64 MiB on Android, with four 16 MiB
  slots. Slot reuse still waits for its completion fence. The old descriptor
  exceeded this device's 128 MiB `maxStorageBufferRange`; other platforms use
  eight slots (128 MiB), with a device-limit check before allocation.
- Android no longer touches all 512 MiB of the GPU shadow buffer at startup.
  Its virtual size remains unchanged. Existing write-watch synchronization
  copies each page before first use; a new regression case checks initial
  contents, dirty updates, and clean-page reuse without prefaulting. Vulkan
  allocation behavior may still commit memory: actual RSS/PSS savings need
  measurement on-device.
- Android image-pool blocks are 32 MiB rather than 64 MiB, reducing spare and
  partly used block overhead. One pipeline worker limits simultaneous driver
  compilation allocations. These trade some potential loading latency for
  lower peak memory use; they do not establish a gameplay performance target.
- Process RSS/swap counters are logged at most once every five seconds.
  These complement texture counters and Android `dumpsys meminfo`; they do
  not measure all GPU allocations or replace PSS measurements.
- Up to 16 rejected pipeline variants per process save their GLSL, cached
  SPIR-V (when available), and pipeline state under
  `files/data/speedbreaker/logs/pipeline_failures`. The normal logs archive
  captures these automatically. Shader linking is still unresolved: the
  upload descriptor fix is independently required, not a proven explanation
  for the driver's shader-link failures.

## Validation and next device test

Build the optimized signed release with `sbDiagnostics=OFF` and `sbBringup=ON`.
The latter retains `run-as` access for test logs. Run the host write-watch
suite in both subpage modes and verify the APK with `apksigner`.

Install over the existing app, preserving the remembered ISO and saves.
Repeat the same Quick Race, car, and track. Check orientation, loading
completion, controls, and at least two minutes of driving. Capture logcat,
the full session-log directory, Android exit information, and memory during
menu and loading. Do not extract or copy the ISO. Do not infer stable gameplay
from build success or a rendered menu.
