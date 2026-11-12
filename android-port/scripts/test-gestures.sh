#!/usr/bin/env bash
set -euo pipefail
PORT_ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
TEST_BUILD="${BUILD_ROOT:-/var/tmp/0ad-android-build}/tests"
mkdir -p "$TEST_BUILD"
"${HOST_CXX:-g++}" -std=c++20 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$PORT_ROOT/native" "$PORT_ROOT/tests/touch_gestures_test.cpp" \
    -o "$TEST_BUILD/touch-gestures"
"$TEST_BUILD/touch-gestures" | tee "$PORT_ROOT/gesture-tests.log"
