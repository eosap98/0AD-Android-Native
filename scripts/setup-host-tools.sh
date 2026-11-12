#!/usr/bin/env bash
# Install only the per-user host tools; Android SDK/JDK/system packages are
# installed separately as documented in BUILD-PC-BARU.md.
source "$(dirname -- "$0")/pc-env.sh"
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
    printf '%s\n' 'Gunakan Linux x86_64; di Windows jalankan melalui WSL2.' >&2
    exit 1
fi
for tool in curl tar unzip sha256sum rustup cc pkg-config; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf '%s\n' "Tool belum terpasang: $tool. Ikuti BUILD-PC-BARU.md." >&2
        exit 1
    fi
done
mkdir -p "$BUILD_ROOT/host-tools" "$BUILD_ROOT/downloads"
fetch_checked() {
    local url="$1" name="$2" checksum="$3"
    local dest="$BUILD_ROOT/downloads/$name"
    if [[ ! -f "$dest" ]]; then
        if [[ -f "$DELIVERY_ROOT/host-tool-cache/$name" ]]; then
            cp -- "$DELIVERY_ROOT/host-tool-cache/$name" "$dest"
        else
            curl -fL --retry 3 --connect-timeout 30 -o "$dest.part" "$url"
            printf '%s  %s\n' "$checksum" "$dest.part" | sha256sum -c -
            mv -- "$dest.part" "$dest"
        fi
    fi
    printf '%s  %s\n' "$checksum" "$dest" | sha256sum -c -
}
fetch_checked \
    'https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.tar.gz' \
    cmake-3.31.6-linux-x86_64.tar.gz \
    5a1133ff103c71eb5120e2cc3de922733e7d8a26a98ae716397e8676adb367bf
tar -xzf "$BUILD_ROOT/downloads/cmake-3.31.6-linux-x86_64.tar.gz" \
    --strip-components=1 -C "$BUILD_ROOT/host-tools"
fetch_checked \
    'https://services.gradle.org/distributions/gradle-8.9-bin.zip' \
    gradle-8.9-bin.zip \
    d725d707bfabd4dfdc958c624003b3c80accc03f7037b5122c4b1d0ef15cecab
unzip -q -o "$BUILD_ROOT/downloads/gradle-8.9-bin.zip" -d "$BUILD_ROOT/host-tools"
rustup toolchain install 1.84.1 --profile minimal --target aarch64-linux-android
# Compile cbindgen on the new host instead of copying a glibc-dependent binary.
RUSTUP_TOOLCHAIN=1.84.1 cargo install --locked --version 0.29.2 \
    --root "$BUILD_ROOT/host-tools/cbindgen" cbindgen
printf '%s\n' 'Host tools siap. Jalankan bash build-pc-baru.sh --check dari root proyek.'
