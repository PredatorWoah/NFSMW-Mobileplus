# 0.1.3 Android audio and GPU measurement test

This follows the first completed RP6 Quick Race sprint in 0.1.2. It is an
experimental test build; engine-audio recovery and RP6 behavior are unverified.
Keep 0.1.2 as the known working race checkpoint and GitHub prerelease.

## Audio routing change

0.1.2 requests six-channel playback; RP6 logs show Android accepts six channels.
This bypasses SDL's downmix even though the handheld has stereo speakers. That
is a concrete routing risk, not proof of why engine audio is absent.

Android now opens stereo playback while paused, then changes only the stream's
input format to the game's existing six-channel float mix. SDL performs the
conversion. Guest channel layout, cadence, queue accounting, gain, and six-channel
raw dumps remain intact. Other platforms retain their existing output request.
`NFSMW_ANDROID_SURROUND=1` restores the previous six-channel request for comparison.
If source-format setup fails, the stream is destroyed and the error logged.

A functional test against pinned SDL 3.2.24 opens a dummy stereo device, sets
six-channel input, then feeds each channel separately. All six contribute to
the output with correct left/right balance; center and LFE feed both sides.
Run via the SB_TESTS audio_downmix_test target, or link tests/audio_downmix_test.cpp
against a host build of the pinned SDL. This verifies conversion behavior, not
RP6 routing or guest engine decoding.

## Diagnostic changes

Android defaults NFSMW_AUDIO_LOG=1, NFSMW_XMA_STATS=1 and NFSMW_GPU_TIMING=1.
Environment overrides are retained. Audio logs aggregate RMS separately for
FL/FR/FC/LFE/SL/SR and count non-finite samples, roughly every two seconds.
Existing aggregate XMA stats report decoding and starvation. No per-sample or
per-context logging, and no automatic PCM dumps.

The renderer checks queue-family timestamp support before allocating its
submission timing query pool, logging enabled/unavailable. Existing presenter
support checks remain. This enables the existing GPU execution measurement;
it does not change render resolution, shaders, target sizes, pass ordering or
synchronization. Timing itself has overhead, so this is a measurement build.

## Next device test

Install over 0.1.2 without clearing app data or copying/extracting the ISO.
Use the same Quick Race/car/track at 720p. Capture logs before launch. Compare
engine sound with music volume set to zero: idle, acceleration, lift-off and
braking, then opponent cars. Check effects and speech too. Record the timestamps
of those actions. Restore music afterwards. If engine sound remains absent,
channel logs plus XMA stats help distinguish silent mixer input from output loss.
The direct ISO reader already logs actual read failures; prior loose-file misses
alone are not evidence of absent packed assets.

Drive two minutes, finish and start another race if stable. Capture memory at
menu, driving, race completion and second race, plus complete session logs and
exit-info before relaunch after failure. Compare GPU executing versus CPU busy
and slot waits before choosing rendering optimizations. Do not infer improved
performance merely from this measurement build.
