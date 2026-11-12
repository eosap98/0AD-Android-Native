#!/usr/bin/env bash
set -euo pipefail
PORT_ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
TEST_BUILD="${BUILD_ROOT:-/var/tmp/0ad-android-build}/tests"
mkdir -p "$TEST_BUILD"
python3 - "${BUILD_ROOT:-/var/tmp/0ad-android-build}/assets/data/mods/public/public.zip" "$TEST_BUILD" <<'PY'
from pathlib import Path
import sys
import zipfile
with zipfile.ZipFile(sys.argv[1]) as archive:
    for source, output in [('input.js', 'session-input.js'), ('selection.js', 'session-selection.js')]:
        (Path(sys.argv[2]) / output).write_bytes(archive.read('gui/session/' + source))
PY
node "$PORT_ROOT/tests/session_input_test.cjs" "$TEST_BUILD/session-input.js" \
    "$PORT_ROOT/native/AndroidTouchInput.cpp" "$TEST_BUILD/session-selection.js" \
    | tee "$PORT_ROOT/session-input-tests.log"
