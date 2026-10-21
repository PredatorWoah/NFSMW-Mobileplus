#!/usr/bin/env bash
# SpeedBreaker One. GPL-3.0-or-later.
#
# Every code guard the runtime checks before touching the game (the
# {address, word} pairs and PPC_LOAD_U32(...) == / Word(...)== checks in
# runtime/game/*.cpp) against the player's own default.xex: a wrong word
# leaves a cheat or fix switched off on the phone. Run where the game APK is
# built, after prepare_recompiler.sh.
#
#   scripts/android/check_guards.sh [path/to/default.xex]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XEX="${1:-$ROOT/game/files/default.xex}"
XR="$ROOT/tools/XenonRecomp"
[ -f "$XEX" ] || { echo "No default.xex at $XEX" >&2; exit 1; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
LIBS=$(find "$XR/build" -name '*.a' ! -path '*XenonAnalyse*')
"${CXX:-clang++}" -std=c++20 -O1 -I"$XR/XenonUtils" -I"$XR/thirdparty" -I"$XR/thirdparty/simde" \
    "$ROOT/scripts/android/xex_words.cpp" "$XR/build/XenonUtils/libXenonUtils.a" $LIBS -o "$WORK/xex_words"
python3 -I - "$ROOT/runtime/game" "$WORK/xex_words" "$XEX" <<'PY'
import glob, re, subprocess, sys
src, tool, xex = sys.argv[1:]
pairs = []
for path in sorted(glob.glob(src + "/*.cpp")):
    text = open(path).read()
    for pattern in (r'\{\s*0x([0-9A-Fa-f]{8})\s*,\s*0x([0-9A-Fa-f]{8})\s*\}',
                    r'PPC_LOAD_U32\(0x([0-9A-Fa-f]{8})\)\s*==\s*0x([0-9A-Fa-f]{8})',
                    r'Word\(0x([0-9A-Fa-f]{8})\)\s*==\s*0x([0-9A-Fa-f]{8})'):
        pairs += [(path.split('/')[-1], a.upper(), w.upper()) for a, w in re.findall(pattern, text)]
out = subprocess.run([tool, xex] + [a for _, a, _ in pairs], capture_output=True, text=True, check=True).stdout.split('\n')
have = dict(line.split() for line in out if line)
bad = [(f, a, w, have.get(a)) for f, a, w in pairs if have.get(a) != w]
for f, a, w, h in bad:
    print(f"MISMATCH {f}: {a} expects {w}, the game has {h}")
print(f"{len(pairs) - len(bad)} of {len(pairs)} guard words match")
sys.exit(1 if bad else 0)
PY
