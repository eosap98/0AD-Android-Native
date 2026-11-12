#!/usr/bin/env bash
source "$(dirname -- "$0")/env.sh"
# Prerequisites are documented in README.md. Existing stages are reused.
python3 "$PORT_ROOT/scripts/fetch-deps.py"
bash "$PORT_ROOT/scripts/build-deps.sh"
bash "$PORT_ROOT/scripts/build-spidermonkey.sh"
bash "$PORT_ROOT/scripts/build-engine.sh"
python3 "$PORT_ROOT/scripts/pack-data.py"
bash "$PORT_ROOT/scripts/build-apk.sh"
