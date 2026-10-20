#!/usr/bin/env bash
# Source this file. All downloaded tools and Gradle state stay in ignored tools/.
if [ -n "${ZSH_VERSION:-}" ]; then
    eval 'SB_ENV_FILE=${(%):-%x}'
else
    SB_ENV_FILE="${BASH_SOURCE[0]}"
fi
SB_LOCAL_ROOT="$(cd "$(dirname "$SB_ENV_FILE")/../.." && pwd)"
export ANDROID_HOME="$SB_LOCAL_ROOT/tools/android-sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/28.2.13676358"
export GRADLE_USER_HOME="$SB_LOCAL_ROOT/tools/gradle-home"
export ANDROID_USER_HOME="$SB_LOCAL_ROOT/tools/android-user"
export JAVA_HOME="$SB_LOCAL_ROOT/tools/jdk-17/Contents/Home"
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmake/3.31.6/bin:$ANDROID_HOME/cmdline-tools/12.0/bin:$PATH"
