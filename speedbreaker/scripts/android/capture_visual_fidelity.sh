#!/usr/bin/env bash
# SpeedBreaker One local-only collector. GPL-3.0-or-later.
set -euo pipefail
PACKAGE=com.onenonlygit.speedbreakerone
TIMEOUT=${SB_CAPTURE_TIMEOUT:-300}
[[ "$TIMEOUT" =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid SB_CAPTURE_TIMEOUT' >&2; exit 2; }
ADB=(adb)
[[ -z "${ANDROID_SERIAL:-}" ]] || ADB+=(-s "$ANDROID_SERIAL")
"${ADB[@]}" get-state >/dev/null
"${ADB[@]}" shell pm path "$PACKAGE" | tr -d '\r' | grep -q '^package:' || { echo 'SpeedBreaker One is not installed' >&2; exit 1; }
OUT=$(realpath -m "${1:-speedbreaker-visual-$(date -u +%Y%m%dT%H%M%SZ)}")
mkdir -p "$OUT"
"${ADB[@]}" shell dumpsys package "$PACKAGE" > "$OUT/package-info.txt"
"${ADB[@]}" exec-out run-as "$PACKAGE" id > "$OUT/run-as.txt" 2> "$OUT/run-as-errors.txt" || { echo 'Extraction requires a debuggable sbBringup=ON build; see run-as-errors.txt' >&2; exit 1; }
VERSION_CODE=$(sed -n 's/.*versionCode=\([0-9]*\).*/\1/p' "$OUT/package-info.txt" | head -n 1)
[[ "$VERSION_CODE" =~ ^[0-9]+$ && "$VERSION_CODE" -ge 6 ]] || { echo 'Install a capture-enabled SpeedBreaker One test build (code 6 or newer).' >&2; exit 1; }
# Start before launch, without clearing previous logcat evidence.
"${ADB[@]}" logcat -v threadtime > "$OUT/logcat.txt" 2> "$OUT/logcat-errors.txt" &
LOG_PID=$!
cleanup() { kill "$LOG_PID" 2>/dev/null || true; wait "$LOG_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM
# Capture old directory list so an earlier completed session cannot satisfy this run.
list_sessions() { "${ADB[@]}" exec-out "run-as $PACKAGE sh -c 'for d in files/data/speedbreaker/diagnostics/visual-*; do [ -d \"\$d\" ] && echo \"\$d\"; done'" | tr -d '\r' || true; }
list_sessions > "$OUT/sessions-before.txt"
"${ADB[@]}" shell am start -n "$PACKAGE/.SpeedBreakerActivity" > "$OUT/launch.txt"
echo 'Start Quick Race and drive briefly. Collecting automatically (up to '"$TIMEOUT"' seconds).'
START=$SECONDS
SESSION=''
while (( SECONDS - START < TIMEOUT )); do
    list_sessions > "$OUT/sessions-current.txt"
    SESSION=$(comm -13 <(sort "$OUT/sessions-before.txt") <(sort "$OUT/sessions-current.txt") | tail -n 1)
    if [[ -n "$SESSION" ]]; then
        "${ADB[@]}" exec-out run-as "$PACKAGE" cat "$SESSION/manifest.json" > "$OUT/manifest-last.json" 2> "$OUT/manifest-errors.txt" || true
        if python3 - "$OUT/manifest-last.json" <<'PY'
import json,sys
try:
    m=json.load(open(sys.argv[1])); sys.exit(0 if m.get('status')=='complete' else 1)
except (OSError,ValueError): sys.exit(1)
PY
        then break; fi
    fi
    sleep 2
done
if [[ -n "$SESSION" ]]; then
    # Exact relative path, no ./ prefix, and never expand a wildcard in tar.
    if "${ADB[@]}" exec-out run-as "$PACKAGE" tar -cf - "$SESSION" > "$OUT/diagnostics.tar" 2> "$OUT/extraction-errors.txt" && tar -tf "$OUT/diagnostics.tar" > "$OUT/diagnostic-files.txt"; then
        tar -xf "$OUT/diagnostics.tar" -C "$OUT"
        rm "$OUT/diagnostics.tar"
    else echo 'Diagnostic extraction failed; see extraction-errors.txt' >&2; fi
else echo 'No new capture session detected before timeout.' | tee "$OUT/capture-status.txt" >&2; fi
"${ADB[@]}" shell dumpsys activity exit-info "$PACKAGE" > "$OUT/exit-info.txt" 2>&1 || true
"${ADB[@]}" shell dumpsys meminfo "$PACKAGE" > "$OUT/memory.txt" 2>&1 || true
cleanup
trap - EXIT INT TERM
ARCHIVE="$OUT.tar.gz"
tar -czf "$ARCHIVE" -C "$(dirname "$OUT")" "$(basename "$OUT")"
echo "Archive: $ARCHIVE"
