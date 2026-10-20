#!/usr/bin/env bash
# Run locally in a terminal. No recording or repeated agent calls required.
set -euo pipefail
PACKAGE=com.onenonlygit.speedbreakerone
SERIAL="${1:?Usage: capture_performance.sh IP:PORT [APK]}"
APK="${2:-}"
command -v adb >/dev/null || { echo 'Install Android platform-tools first.' >&2; exit 1; }
adb connect "$SERIAL"
adb -s "$SERIAL" get-state
if [[ -n "$APK" ]]; then adb -s "$SERIAL" install -r "$APK"; fi
OUT="${SB_CAPTURE_DIR:-$PWD/SpeedBreaker-0.1.11-$(date +%Y%m%d-%H%M%S)}"
mkdir -p "$OUT"
adb -s "$SERIAL" shell am force-stop "$PACKAGE"
adb -s "$SERIAL" logcat -c
adb -s "$SERIAL" logcat -v threadtime SpeedBreakerOne:V hook_impl:V SDL:V AndroidRuntime:E libc:F DEBUG:F '*:S' > "$OUT/logcat.txt" 2>&1 &
LOGGER=$!
cleanup() { kill "$LOGGER" 2>/dev/null || true; wait "$LOGGER" 2>/dev/null || true; }
trap cleanup EXIT
adb -s "$SERIAL" exec-out run-as "$PACKAGE" cat files/data/speedbreaker/settings.toml > "$OUT/settings-before.toml" 2>/dev/null || true
adb -s "$SERIAL" shell dumpsys package "$PACKAGE" > "$OUT/package.txt"
adb -s "$SERIAL" exec-out run-as "$PACKAGE" cat shared_prefs/gpu_driver.xml > "$OUT/driver-selection.xml" 2>/dev/null || true
adb -s "$SERIAL" shell am start -n "$PACKAGE/.LauncherActivity"
printf '\n0.1.11: automatic per-pass GPU timestamps, no recording/readback.\nChoose the driver on the launch screen and press Play.\nUse 720p / Frame Rate 60; keep GPU Scheduling and Work Reduction unchanged.\nKeep Shader Predicate Coalescing and Depth Stage Reduction ON for the first run.\nFor rollback comparisons switch one OFF, then restart from the launch screen.\nWarm up for 60 seconds, then drive the same Quick Race for 2 minutes.\nNote FPS, GPU usage and hitches. Start a second race.\nOptional shader check: switch Advanced > Asynchronous Shaders OFF (live),\nthen repeat the same warmed race and note whether the hitches change.\nThe first encounter may compile shaders; compare warmed runs.\nType done when finished. The collector will press Home and wait 10 seconds,\nthen package the pass timings, shader diagnostics and background logs.\n\n'
IFS= read -r -t 600 ANSWER || true
adb -s "$SERIAL" exec-out run-as "$PACKAGE" cat files/data/speedbreaker/settings.toml > "$OUT/settings-after.toml" 2>/dev/null || true
adb -s "$SERIAL" exec-out run-as "$PACKAGE" cat shared_prefs/gpu_driver.xml > "$OUT/driver-selection-after.xml" 2>/dev/null || true
adb -s "$SERIAL" shell dumpsys cpuinfo > "$OUT/cpu-before-home.txt" || true
adb -s "$SERIAL" shell input keyevent KEYCODE_HOME
sleep 10
adb -s "$SERIAL" shell dumpsys thermalservice > "$OUT/thermal.txt" || true
adb -s "$SERIAL" shell dumpsys meminfo "$PACKAGE" > "$OUT/memory.txt" || true
adb -s "$SERIAL" shell dumpsys activity exit-info "$PACKAGE" > "$OUT/exit-info.txt" || true
adb -s "$SERIAL" shell dumpsys battery > "$OUT/battery.txt" || true
adb -s "$SERIAL" shell dumpsys cpuinfo > "$OUT/background-cpu.txt" || true
adb -s "$SERIAL" exec-out run-as "$PACKAGE" tar -cf - files/data/speedbreaker/logs > "$OUT/session-logs.tar"
tar -tf "$OUT/session-logs.tar" > "$OUT/session-log-files.txt"
[[ -s "$OUT/session-log-files.txt" ]] || { echo 'No app logs were extracted.' >&2; exit 1; }
cleanup
trap - EXIT
if grep -q '\[gpu-pass\] output .* (ready)' "$OUT/logcat.txt"; then
    echo 'Per-pass GPU timing output confirmed for this run.' | tee "$OUT/pass-timing-check.txt"
else
    echo 'WARNING: No current per-pass output confirmed. Keep this archive: check APK/version, timestamp support and race activity.' | tee "$OUT/pass-timing-check.txt"
fi
tar -czf "$OUT.tar.gz" -C "$(dirname "$OUT")" "$(basename "$OUT")"
printf '\nCapture ready: %s.tar.gz\n' "$OUT"
