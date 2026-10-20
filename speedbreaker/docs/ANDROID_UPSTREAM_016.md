# 0.1.6 selective upstream integration

Baseline `5f057d5`; upstream v0.1.1 `05838d6b8b0c005ed266db1f6d4af23e5dd555fe`.
Compared the v0.1.0 -> v0.1.1 change and our Android renderer/checkpoints.

| Upstream change | Decision / reason |
| --- | --- |
| `depth24_glsl.h`, resolve integration and `depth24_pack_test.cpp` | Import verbatim header/test and minimal renderer splice. Clamp integer depth before shift; preserve stencil convention and other formats. Test checks exact GLSL/C++ tokens and renderer integration. |
| Additional shadow-map readback/probe | Defer: 0.1.5 already has bounded depth/texture/final-frame capture. Another probe changes workload and is unnecessary for proving the overflow. |
| `memory_plan.h` plus guest/ring allocation overhaul | Defer: handles desktop no-BAR and Intel import failures, adds cross-platform fallback policy. Android currently races; preserve split memory, lazy shadow pages, 64 MiB ring and four slots. Revisit only after memory-type/allocation evidence. No claim that the helper is inherently incompatible with Android. |
| Taller-screen aspect threshold and edge-bar drawing | Defer: RP6 landscape 16:9 does not need these fixes; bar changes couple extra draws with cache state updates. Avoid unrelated visual/presentation changes in this lighting comparison. |
| Installer placement/folder picker | Defer: preserve Android content URI/direct ISO semantics. Desktop folder placement is not a replacement for Storage Access Framework permissions. |
| Online updater/QR and associated UI/settings/build deps | Defer: upstream binaries/updater target other platforms; Android package identity and signing/release flow differ. |
| Virtual pad scripted buttons/screenshots | Defer: no unattended game input needed for offline build. Preserve controller behavior. |
| macOS minimum version, bundles, SteamOS launcher and dependency notices | No applicable Android code imported. Existing pinned Android deps/licenses retained; imported shared files retain GPL attribution. |

## Post-fix shadow flicker

Upstream issue #3 comment at 2026-10-10 19:54:23Z reports shadows "flickering and
disappearing" after the fix. This is a user observation, without attached capture,
build/driver/scene details or a confirmed cause. Source:
https://github.com/SpeedBreakerProject/speedbreaker/issues/3#issuecomment-6101564905
The maintainer earlier reported sunlight parity with actual Xbox 360 footage;
that is upstream evidence, not independent RP6 verification here.

The imported exhaustive test shows only `1.0` changes in the upper input range,
under separate or fused arithmetic; monotonicity holds and quantization error
stays within one LSB. This weakens a hypothesis of a new broad packing discontinuity.
It does not prove GPU resource lifetime, moving shadow projections or all GLSL
NaN behavior. NaN semantics are undefined in GLSL; finite depth remains the invariant.

Trace `ClearRect` -> shadow draw depth state -> `Resolve` -> tiled guest word /
fused texture -> cached `GetTexture` -> shader comparison. Candidate causes:
(1) normal moving shadow-map coverage/LOD or bias now visible after sunlight fix;
(2) stale resolve texture or alias invalidation (`loadedSequence`, partial-range
`keep` checks and write-watch ownership); (3) pass-area/clear expansion or depth
state mismatch; (4) D24FS8 representation or depth exports **if used**; (5) pending
pipelines skipped while warming (logged `pipelinePending`). Shared upstream
observation makes Android-only area policy a weaker universal explanation, but
it still needs separate validation on RP6. No candidate is confirmed.

Cheapest next discriminator: existing 0.1.5 captures in the 0.1.6 APK plus the
private session log, checking clear/source far depths and matching resolved words,
sampled textures, format, projection constants and pending shader counts. Snapshot
frames are spaced and may miss rapid flicker; a short timestamped screen clip is
only needed if this occurs. Do not alter bias, PCF, precision or synchronization
without that evidence. No speculative flicker fix implemented.

## Comparison source identity

nfsmw-nx: `be2d4c67ab42d6d629746a01dbf03cef824c8d2d`, inspected docs/performance-history.md, docs/toolchain.md, docs/measuring.md and renderer/shader source.
