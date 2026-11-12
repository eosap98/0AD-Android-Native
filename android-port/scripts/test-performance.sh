#!/usr/bin/env bash
set -euo pipefail
PORT_ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
TEST_BUILD="${BUILD_ROOT:-/var/tmp/0ad-android-build}/tests"
JAVA_TOOLS="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}/bin"
mkdir -p "$TEST_BUILD/performance-java"
{
    "${HOST_CXX:-g++}" -std=c++20 -O1 -g -Wall -Wextra -Werror \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -I "$PORT_ROOT/native" "$PORT_ROOT/tests/frame_pacer_test.cpp" \
        -o "$TEST_BUILD/frame-pacer"
    "$TEST_BUILD/frame-pacer"
    "$JAVA_TOOLS/javac" --release 17 -d "$TEST_BUILD/performance-java" \
        "$PORT_ROOT/app/src/main/java/org/wildfiregames/zeroad/PerformanceProfile.java" \
        "$PORT_ROOT/tests/PerformanceProfileTest.java"
    "$JAVA_TOOLS/java" -cp "$TEST_BUILD/performance-java" org.wildfiregames.zeroad.PerformanceProfileTest
} | tee "$PORT_ROOT/performance-tests.log"
