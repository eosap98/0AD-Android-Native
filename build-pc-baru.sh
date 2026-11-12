#!/usr/bin/env bash
set -euo pipefail
DELIVERY_ROOT="$(cd -- "$(dirname -- "$0")" && pwd)"
source "$DELIVERY_ROOT/scripts/pc-env.sh"
case "${1:-}" in
    --help|-h)
        printf '%s\n' 'Build: bash build-pc-baru.sh' \
            'Periksa toolchain saja: bash build-pc-baru.sh --check' \
            'Prasyarat dan persiapan: BUILD-PC-BARU.md'
        exit 0 ;;
    --check)
        python3 "$DELIVERY_ROOT/scripts/check-build-env.py"
        exit 0 ;;
    '') ;;
    *) printf '%s\n' "Argumen tidak dikenal: $1" >&2; exit 2 ;;
esac
python3 "$DELIVERY_ROOT/scripts/check-build-env.py"
python3 "$DELIVERY_ROOT/scripts/prepare-sources.py"
# The private backup supplies the same SHA-256-pinned dependency downloads.
if [[ -d "$DELIVERY_ROOT/dependency-cache" ]]; then
    mkdir -p "$BUILD_ROOT/downloads"
    for archive in "$DELIVERY_ROOT"/dependency-cache/*.tar; do
        [[ -f "$archive" ]] || continue
        target="$BUILD_ROOT/downloads/$(basename -- "$archive")"
        if [[ ! -e "$target" ]]; then cp -- "$archive" "$target"; fi
    done
fi
bash "$DELIVERY_ROOT/android-port/scripts/build-all.sh"
