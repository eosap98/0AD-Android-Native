#!/usr/bin/env bash
source "$(dirname -- "$0")/env.sh"
SM="$BUILD_ROOT/src/mozjs-128.13.0"
if [[ -f "$BUILD_ROOT/spidermonkey.done" ]]; then exit 0; fi
if [[ ! -d "$SM" ]]; then
    tar -xJf "$GAME_SRC/libraries/source/spidermonkey/mozjs-128.13.0.tar.xz" \
        -C "$BUILD_ROOT/src" --exclude='*/js/src/tests/non262' \
        --exclude='*/js/src/tests/test262' --exclude='*/js/src/jit-test' \
        --exclude='*/python/mozperftest' --exclude='*/testing/web-platform'
fi
if [[ ! -f "$SM/.patched-0ad" ]]; then
    for p in FixRustLinkage.diff FixLibNames.diff FixPkgConfigDebug.diff \
             FixExtraGCTracing.diff SuppressDanglingPointerWarning.patch FixPython3_14.diff; do
        patch -d "$SM" -p1 < "$GAME_SRC/libraries/source/spidermonkey/patches/$p"
    done
    touch "$SM/.patched-0ad"
fi
if [[ ! -f "$SM/.patched-android" ]]; then
    patch -d "$SM" -p1 < "$PORT_ROOT/patches/mozjs-ndk27.patch"
    touch "$SM/.patched-android"
fi
export MOZ_NOSPAM=1
# The release source archive contains a pre-generated old-configure with an
# obsolete source-directory probe. Regenerate it from its current aclocal.m4.
if [[ ! -f "$SM/.regenerated-configure" ]]; then
    rm -f "$SM/js/src/old-configure"
    touch "$SM/.regenerated-configure"
fi
export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none
export MOZBUILD_STATE_PATH="$BUILD_ROOT/mozbuild-state"
export MOZCONFIG="$BUILD_ROOT/mozconfig"
export HOST_CC=/usr/bin/clang
export HOST_CXX=/usr/bin/clang++
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$CC"
export RUSTUP_TOOLCHAIN="${RUSTUP_TOOLCHAIN:-1.84.1}"
if [[ -x "$BUILD_ROOT/host-tools/rust-1.84.1/bin/rustc" ]]; then
    export RUSTC="$BUILD_ROOT/host-tools/rust-1.84.1/bin/rustc"
    export CARGO="$BUILD_ROOT/host-tools/rust-1.84.1/bin/cargo"
else
    export RUSTC="$(rustup which rustc)"
    export CARGO="$(rustup which cargo)"
fi
cat > "$MOZCONFIG" <<EOF
ac_add_options --enable-project=js
ac_add_options --enable-shared-js
ac_add_options --disable-jemalloc
ac_add_options --without-intl-api
ac_add_options --disable-js-shell
ac_add_options --disable-tests
ac_add_options --disable-debug
ac_add_options --disable-debug-symbols
ac_add_options --disable-bootstrap
ac_add_options --enable-optimize=-O2
ac_add_options --target=aarch64-linux-android
ac_add_options --host=x86_64-unknown-linux-gnu
ac_add_options --with-android-ndk=$ANDROID_NDK_ROOT
ac_add_options --with-android-version=29
mk_add_options MOZ_OBJDIR=$BUILD_ROOT/obj/spidermonkey
EOF
cd "$SM"
./mach build -j "$JOBS"
mkdir -p "$PREFIX/include/mozjs-128" "$PREFIX/lib"
cp -aL "$BUILD_ROOT/obj/spidermonkey/dist/include/." "$PREFIX/include/mozjs-128/"
cp -L "$BUILD_ROOT/obj/spidermonkey/dist/bin/libmozjs-128.so" "$PREFIX/lib/"
touch "$BUILD_ROOT/spidermonkey.done"
