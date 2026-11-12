#!/usr/bin/env bash
set -euo pipefail
PORT_ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
TEST_BUILD="${BUILD_ROOT:-/var/tmp/0ad-android-build}/tests"
mkdir -p "$TEST_BUILD/scene-stubs"
# Compile the production scene-buffer source against recorded engine services.
# Stub include files live only in this host-test directory, outside engine builds.
python3 - "$TEST_BUILD/scene-stubs" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1])
for name in ['precompiled.h', 'graphics/Texture.h', 'graphics/Canvas2D.h',
             'graphics/Color.h', 'graphics/TextureManager.h', 'lib/ogl.h',
             'maths/Rect.h', 'ps/CLogger.h', 'renderer/backend/Backend.h',
             'renderer/backend/IDevice.h', 'renderer/backend/IFramebuffer.h',
             'renderer/backend/ITexture.h', 'renderer/backend/Sampler.h']:
    p=root/name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text('#pragma once\n')
PY
"${HOST_CXX:-g++}" -std=c++20 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$TEST_BUILD/scene-stubs" -I "$PORT_ROOT/native" \
    -include "$PORT_ROOT/tests/SceneTestEngine.h" \
    "$PORT_ROOT/native/AndroidSceneBuffer.cpp" "$PORT_ROOT/tests/scene_buffer_test.cpp" \
    -o "$TEST_BUILD/scene-buffer"
"$TEST_BUILD/scene-buffer" | tee "$PORT_ROOT/rendering-tests.log"
