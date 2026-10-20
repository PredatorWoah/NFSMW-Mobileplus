#!/usr/bin/env bash
# Workspace-local Apple Silicon Android toolchain; no Homebrew/system install.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
mkdir -p tools/bootstrap tools/android-sdk/cmdline-tools tools/android-user
if [ ! -x tools/jdk-17/Contents/Home/bin/java ]; then
    curl -fsSL --retry 3 'https://api.adoptium.net/v3/assets/latest/17/hotspot?architecture=aarch64&image_type=jdk&os=mac' -o tools/bootstrap/jdk17-mac.json
    python3 - <<'PY'
import hashlib, json, pathlib, subprocess, tarfile
root = pathlib.Path('tools/bootstrap')
p = json.loads((root/'jdk17-mac.json').read_text())[0]['binary']['package']
archive = root / p['name']
if not archive.exists():
    subprocess.run(['curl', '-fsSL', '--retry', '3', p['link'], '-o', str(archive)], check=True)
if hashlib.sha256(archive.read_bytes()).hexdigest() != p['checksum']:
    raise SystemExit('JDK SHA-256 mismatch')
with tarfile.open(archive) as t:
    top = t.getnames()[0].split('/')[0]
    t.extractall(root)
(root/top).rename('tools/jdk-17')
print('Verified JDK SHA-256:', p['checksum'])
PY
fi
if [ ! -x tools/android-sdk/cmdline-tools/12.0/bin/sdkmanager ]; then
    ARCHIVE=tools/bootstrap/commandlinetools-mac-11076708_latest.zip
    [ -f "$ARCHIVE" ] || curl -fsSL --retry 3 https://dl.google.com/android/repository/commandlinetools-mac-11076708_latest.zip -o "$ARCHIVE"
    curl -fsSL --retry 3 https://dl.google.com/android/repository/repository2-1.xml -o tools/bootstrap/android-repository.xml
    python3 - <<'PY'
import hashlib, pathlib, xml.etree.ElementTree as ET, zipfile
root=pathlib.Path('tools/bootstrap')
archive=root/'commandlinetools-mac-11076708_latest.zip'
tree=ET.parse(root/'android-repository.xml')
checks=[a.findtext('complete/checksum') for a in tree.findall('.//archive')
        if a.findtext('complete/url')==archive.name]
if len(checks)!=1 or hashlib.sha1(archive.read_bytes()).hexdigest()!=checks[0]:
    raise SystemExit('Android command-line archive checksum mismatch or missing official metadata')
with zipfile.ZipFile(archive) as z:
    z.extractall(root/'commandline-unpack')
(root/'commandline-unpack/cmdline-tools').rename('tools/android-sdk/cmdline-tools/12.0')
for p in pathlib.Path('tools/android-sdk/cmdline-tools/12.0/bin').iterdir():
    p.chmod(0o755)
print('Verified Android command-line archive against official repository checksum')
PY
fi
source scripts/android/macos_env.sh
java -version
# Standard Android SDK licenses for the requested local build.
{ yes || true; } | sdkmanager --sdk_root="$ANDROID_HOME" --licenses > tools/bootstrap/sdk-licenses.log 2>&1
PACKAGES=('platform-tools' 'platforms;android-35' 'build-tools;35.0.0' 'cmake;3.31.6')
# A verified NDK archive may already be downloading separately. Avoid a second
# 952 MB transfer; the native build still waits for the NDK to finish.
if [ "${1:-}" != --without-ndk ]; then PACKAGES+=('ndk;28.2.13676358'); fi
sdkmanager --sdk_root="$ANDROID_HOME" "${PACKAGES[@]}" > tools/bootstrap/sdk-install.log 2>&1
cmake --version
ninja --version
adb devices -l
