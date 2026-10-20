#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUTPUT="$(mktemp -d)"
trap 'rm -rf "$OUTPUT"' EXIT
"${JAVA_HOME:+$JAVA_HOME/bin/}javac" -d "$OUTPUT" \
    "$ROOT/android/app/src/main/java/com/onenonlygit/speedbreakerone/DriverArchive.java" \
    "$ROOT/tests/android/DriverArchiveTest.java"
"${JAVA_HOME:+$JAVA_HOME/bin/}java" -cp "$OUTPUT" com.onenonlygit.speedbreakerone.DriverArchiveTest
