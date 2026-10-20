#!/usr/bin/env bash
# Read-only collection for the game-free APK. Private phone logs never enter Git.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ADB="${ADB:-$ROOT/tools/android-sdk/platform-tools/adb}"
PACKAGE=com.onenonlygit.speedbreakerone
DEST="$ROOT/private/vivo-x300-pro/probe-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$DEST"
"$ADB" -d get-state >/dev/null
"$ADB" -d shell getconf PAGE_SIZE > "$DEST/page-size.txt"
"$ADB" -d shell getprop ro.product.model > "$DEST/model.txt"
"$ADB" -d shell getprop ro.build.version.release > "$DEST/android-version.txt"
"$ADB" -d shell cmd gpu vkjson > "$DEST/vkjson.json"
"$ADB" -d shell run-as "$PACKAGE" cat files/android-diagnostics.log > "$DEST/android-diagnostics.log"
"$ADB" -d logcat -d -v threadtime -s SpeedBreakerOne SDL AndroidRuntime > "$DEST/logcat.txt"
"$ADB" -d shell dumpsys meminfo "$PACKAGE" > "$DEST/meminfo.txt"
"$ADB" -d shell dumpsys activity exit-info "$PACKAGE" > "$DEST/exit-info.txt"
printf 'Saved private platform report to %s\n' "$DEST"
cat "$DEST/android-diagnostics.log"
