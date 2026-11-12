#!/usr/bin/env bash
# Source this file before invoking the original build stages directly.
set -euo pipefail
DELIVERY_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export DELIVERY_ROOT
export BUILD_ROOT="${BUILD_ROOT:-$HOME/0ad-android-build/android.4}"
export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/27.2.12479018}"
export GAME_SRC="${GAME_SRC:-$DELIVERY_ROOT/0ad-0.28.0-unix-build/0ad-0.28.0}"
export RUSTUP_TOOLCHAIN="${RUSTUP_TOOLCHAIN:-1.84.1}"
export JOBS="${JOBS:-3}"
export PATH="$BUILD_ROOT/host-tools/bin:$BUILD_ROOT/host-tools/usr/bin:$BUILD_ROOT/host-tools/cbindgen/bin:$BUILD_ROOT/host-tools/rust-1.84.1/bin:$HOME/.cargo/bin:$ANDROID_SDK_ROOT/cmake/3.22.1/bin:$PATH"
export CMAKE="${CMAKE:-$BUILD_ROOT/host-tools/bin/cmake}"
export GRADLE="${GRADLE:-$BUILD_ROOT/host-tools/gradle-8.9/bin/gradle}"
if [[ -z "${JAVA_HOME:-}" ]]; then
    if command -v javac >/dev/null 2>&1; then
        JAVA_HOME="$(dirname -- "$(dirname -- "$(readlink -f -- "$(command -v javac)")")")"
    else
        JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
    fi
fi
export JAVA_HOME
export PATH="$JAVA_HOME/bin:$PATH"
