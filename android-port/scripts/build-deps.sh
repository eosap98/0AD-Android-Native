#!/usr/bin/env bash
source "$(dirname -- "$0")/env.sh"

cmake_dep() {
    local name="$1"
    shift
    if [[ -f "$BUILD_ROOT/$name.done" ]]; then return; fi
    echo "Building $name"
    "$CMAKE" -S "$BUILD_ROOT/src/$name" -B "$BUILD_ROOT/obj/$name" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_FIND_ROOT_PATH="$PREFIX" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS" -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" "$@"
    "$CMAKE" --build "$BUILD_ROOT/obj/$name" -j "$JOBS"
    "$CMAKE" --install "$BUILD_ROOT/obj/$name"
    touch "$BUILD_ROOT/$name.done"
}

autoconf_dep() {
    local name="$1"
    shift
    if [[ -f "$BUILD_ROOT/$name.done" ]]; then return; fi
    echo "Building $name"
    mkdir -p "$BUILD_ROOT/obj/$name"
    ( cd "$BUILD_ROOT/obj/$name"
      "$BUILD_ROOT/src/$name/configure" --host=aarch64-linux-android \
          --prefix="$PREFIX" --disable-shared --enable-static "$@"
      make -j "$JOBS"
      make install )
    touch "$BUILD_ROOT/$name.done"
}

cmake_dep sdl -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF
cmake_dep fmt -DFMT_TEST=OFF -DFMT_DOC=OFF
cmake_dep png -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF
cmake_dep freetype -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_BZIP2=ON
cmake_dep xml -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_PROGRAMS=OFF -DLIBXML2_WITH_TESTS=OFF \
    -DLIBXML2_WITH_ICONV=OFF -DLIBXML2_WITH_ICU=OFF -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_HTTP=OFF
cmake_dep enet -DENET_BUILD_TESTS=OFF
cmake_dep ogg -DBUILD_TESTING=OFF
cmake_dep vorbis -DBUILD_TESTING=OFF
cmake_dep openal -DLIBTYPE=STATIC -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF \
    -DALSOFT_TESTS=OFF -DALSOFT_INSTALL_CONFIG=OFF -DALSOFT_INSTALL_HRTF_DATA=OFF \
    -DALSOFT_BACKEND_OPENSL=ON -DALSOFT_BACKEND_OBOE=OFF
cmake_dep mbedtls -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF
cmake_dep curl -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DCURL_USE_MBEDTLS=ON \
    -DCURL_USE_OPENSSL=OFF -DCURL_USE_LIBPSL=OFF -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF \
    -DCURL_USE_LIBSSH2=OFF -DCURL_USE_LIBSSH=OFF -DHTTP_ONLY=ON
autoconf_dep sodium --disable-ssp
autoconf_dep iconv

if [[ ! -f "$BUILD_ROOT/icu.done" ]]; then
    mkdir -p "$BUILD_ROOT/obj/icu-host" "$BUILD_ROOT/obj/icu"
    ( cd "$BUILD_ROOT/obj/icu-host"
      env -u CPPFLAGS -u LDFLAGS CC=cc CXX=c++ AR=ar RANLIB=ranlib CFLAGS=-O2 CXXFLAGS=-O2 \
          "$BUILD_ROOT/src/icu/source/configure" --disable-tests --disable-samples --disable-extras
      make -j "$JOBS" )
    ( cd "$BUILD_ROOT/obj/icu"
      "$BUILD_ROOT/src/icu/source/configure" --host=aarch64-linux-android \
          --with-cross-build="$BUILD_ROOT/obj/icu-host" --prefix="$PREFIX" \
          --disable-shared --enable-static --disable-tests --disable-samples --disable-extras \
          --with-data-packaging=static
      make -j "$JOBS"
      make install )
    touch "$BUILD_ROOT/icu.done"
fi
echo "Native dependency build complete."
