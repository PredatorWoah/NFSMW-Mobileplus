#!/usr/bin/env bash
# Never upload the executable or generated code to git or public CI artifacts.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
[ $# -eq 1 ] || { echo 'usage: generate_ppc.sh /path/to/your/USA/default.xex' >&2; exit 2; }
EXPECTED=aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c
if command -v sha256sum >/dev/null; then
    ACTUAL="$(sha256sum "$1" | cut -d' ' -f1)"
else
    ACTUAL="$(shasum -a 256 "$1" | cut -d' ' -f1)"
fi
[ "$ACTUAL" = "$EXPECTED" ] || { echo 'Unsupported executable: USA base release hash does not match' >&2; exit 1; }
[ -x "$ROOT/tools/XenonRecomp/build/XenonRecomp/XenonRecomp" ] || { echo 'Run scripts/android/prepare_recompiler.sh first' >&2; exit 1; }
mkdir -p "$ROOT/game/files" "$ROOT/ppc"
if [ "$(realpath "$1")" != "$ROOT/game/files/default.xex" ]; then cp "$1" "$ROOT/game/files/default.xex"; fi
cd "$ROOT"
tools/XenonRecomp/build/XenonRecomp/XenonRecomp config/nfsmw.toml tools/XenonRecomp/XenonUtils/ppc_context.h
