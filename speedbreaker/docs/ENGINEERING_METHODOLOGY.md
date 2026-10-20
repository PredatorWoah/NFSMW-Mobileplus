# Mandatory engineering methodology

Applies to every future debugging, porting and optimization task in SpeedBreaker One.
Read this document and the current Android checkpoints before changing code. User
instructions determine task scope; these rules determine how conclusions are reached.

1. **Verify fundamentals first.** Check numerical boundaries, bit widths, signedness,
   rounding, data formats, endian order, ownership and memory semantics. Write down
   assumptions and the invariant they imply before creating instrumentation.
2. **Source analysis before experimentation.** Trace input, transformation, storage,
   synchronization and consumption. Locate the first violated invariant, rather than
   changing the final symptom. Read actual code, not only comments or log labels.
3. **Form competing hypotheses.** Include another subsystem and shared-code causes.
   Specify an observation that would falsify each hypothesis; seek that observation.
4. **Simplest test first.** Use reproducible calculations and isolated tests before
   captures, instrumentation or APK builds. Exercise the expression actually compiled,
   including plausible fused arithmetic, instead of testing a different implementation.
5. **Distinguish shared from platform-specific bugs.** A defect on desktop and Android
   sends investigation to shared code first. Driver differences remain hypotheses
   until a controlled comparison supports them.
6. **Evidence over intuition.** Label observations, confirmed defects, hypotheses and
   measured improvements separately. Record provenance, build, device, scene and
   limitations. A passing host test or build is not an Adreno result.
7. **One variable at a time.** Preserve known-good binaries and inputs. Keep changes
   independently reversible and in separate commits. Compare equivalent warm scenes,
   settings and thermal conditions; characterize noise before claiming a small gain.
8. **Preserve correctness.** No optimization may silently degrade graphics, audio,
   stability or gameplay. Dropping casters, shader work, synchronization or game updates
   is not an acceptable default shortcut. Define acceptance checks and rollback first.
9. **Measure before optimizing.** Identify the critical path using frame times, thread
   CPU time, GPU timestamps, utilization, synchronization, memory and power where data
   exists. Wait duration is not proof of GPU saturation. Distinguish cumulative from
   interval counters, CPU wall time from actual CPU time, and elapsed GPU intervals
   from shader-only execution. Exclude capture readbacks and warm-up from benchmarks.
10. **Document failed hypotheses.** Record the test, result, interpretation and rejection
    reason. An untested possibility is not disproven; a feature with zero attempts
    has not been validated. Preserve negative results in the appropriate checkpoint.

## Required task record

For each substantive change, record: baseline/source identity; observed defect;
complete data path and invariant; competing hypotheses and falsification tests;
cheapest discriminating test; isolated change/rollback; verification actually run;
remaining uncertainty; and the single most valuable next test. Reuse existing
checkpoints and tests; build only when it answers a question or delivers a release.
Protect XEX/assets, generated game code, credentials and private diagnostic archives.

## Worked example: far-plane shadow depth

The shared resolve used `uint(clamp(d,0,1)*16777215.0+0.5)<<8`.
At `d=1.0`, binary32 rounds the sum to `16777216`; shifting eight bits wraps
it to zero. Trace: D32 depth attachment -> resolve `pack32` formats 22/23 ->
tiled guest word and fused texture -> UNORM unpack -> shadow comparison.
An empty far-plane texel became nearest depth. Upstream v0.1.1 clamps the integer
to `0xFFFFFF` before shifting. Its test evaluates the same tokens used in GLSL,
checks separate/FMA rounding, exhaustive upper-range inputs and renderer integration.
This proves a shared packing defect; Android visual recovery still requires testing.
D24FS8 representation and fragment depth exports are separate open questions.
