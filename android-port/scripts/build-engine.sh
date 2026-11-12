#!/usr/bin/env bash
source "$(dirname -- "$0")/env.sh"
if [[ ! -d "$BUILD_ROOT/src/cxxtest" ]]; then
    mkdir -p "$BUILD_ROOT/src/cxxtest"
    tar -xf "$GAME_SRC/libraries/source/cxxtest-4.4/cxxtest-4.4.tar.gz" \
        -C "$BUILD_ROOT/src/cxxtest" --strip-components=1
fi
"$CMAKE" -S "$PORT_ROOT/native" -B "$BUILD_ROOT/obj/engine" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release -DGAME_SRC="$GAME_SRC" -DBUILD_ROOT="$BUILD_ROOT"
"$CMAKE" --build "$BUILD_ROOT/obj/engine" -j "$JOBS"
mkdir -p "$BUILD_ROOT/jniLibs/arm64-v8a"
for lib in "$BUILD_ROOT/obj/engine/libpyrogenesis.so" "$PREFIX/lib/libSDL2.so" \
           "$PREFIX/lib/libmozjs-128.so" \
           "$TOOLCHAIN/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"; do
    cp -L "$lib" "$BUILD_ROOT/jniLibs/arm64-v8a/"
done
"$STRIP" --strip-unneeded "$BUILD_ROOT"/jniLibs/arm64-v8a/*.so
