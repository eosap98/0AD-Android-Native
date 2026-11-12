cmake_minimum_required(VERSION 3.22)
project(zeroad_android LANGUAGES C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This port targets Android arm64-v8a")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(GAME_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../0ad-0.28.0-unix-build/0ad-0.28.0" CACHE PATH "0 A.D. release source")
set(BUILD_ROOT "/var/tmp/0ad-android-build" CACHE PATH "Native dependencies")
set(PREFIX "${BUILD_ROOT}/prefix")
set(SRC "${GAME_SRC}/source")

# These are the same nonrecursive source groups used by the release premake
# project. Keep tests, desktop platform implementations, and editor UI out.
set(SOURCE_DIRS
    network network/scripting rlinterface
    lobby/scripting third_party/encryption
    simulation2 simulation2/components simulation2/helpers simulation2/scripting
    simulation2/serialization simulation2/system simulation2/testcomponents
    scriptinterface scriptinterface/third_party
    ps ps/scripting ps/GameSetup ps/XMB ps/XML
    soundmanager soundmanager/data soundmanager/items soundmanager/scripting
    maths maths/scripting i18n i18n/scripting
    graphics graphics/scripting renderer renderer/backend renderer/backend/dummy
    renderer/backend/gl renderer/backend/vulkan renderer/scripting
    third_party/mikktspace third_party/ogre3d_preprocessor third_party/vma
    tools/atlas/GameInterface tools/atlas/GameInterface/Handlers
    gui gui/ObjectTypes gui/ObjectBases gui/Scripting gui/SettingTypes
    lib lib/adts lib/allocators lib/external_libraries lib/file lib/file/archive
    lib/file/common lib/file/io lib/file/vfs lib/pch lib/posix lib/res lib/res/graphics
    lib/sysdep lib/tex lib/sysdep/arch/aarch64 lib/sysdep/os/linux
    lib/sysdep/os/unix lib/sysdep/rtl/gcc)
set(ENGINE_SOURCES "${SRC}/main.cpp" "${SRC}/lobby/Globals.cpp")
foreach(DIR IN LISTS SOURCE_DIRS)
    file(GLOB GROUP CONFIGURE_DEPENDS "${SRC}/${DIR}/*.cpp" "${SRC}/${DIR}/*.c")
    list(APPEND ENGINE_SOURCES ${GROUP})
endforeach()
file(GLOB MOCKS "${SRC}/mocks/*_real.cpp")
list(APPEND ENGINE_SOURCES ${MOCKS})
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/precompiled\\.cpp$")

add_library(pyrogenesis SHARED ${ENGINE_SOURCES} android_bridge.cpp AndroidTouchInput.cpp AndroidSceneBuffer.cpp)
target_include_directories(pyrogenesis PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${SRC}" "${SRC}/pch/engine"
    "${SRC}/third_party/glad/include" "${SRC}/third_party/tinygettext/include"
    "${SRC}/third_party/mongoose" "${SRC}/tools/atlas"
    "${BUILD_ROOT}/src/cxxtest"
    "${BUILD_ROOT}/src/boost" "${PREFIX}/include"
    "${PREFIX}/include/SDL2" "${PREFIX}/include/libxml2"
    "${PREFIX}/include/mozjs-128" "${PREFIX}/include/freetype2"
    "${BUILD_ROOT}/obj/spidermonkey/dist/include"
    "${BUILD_ROOT}/src/icu/source/common" "${BUILD_ROOT}/src/icu/source/i18n")
target_compile_definitions(pyrogenesis PRIVATE
    NDEBUG CONFIG_FINAL=1 CONFIG_ENABLE_PCH=1
    CONFIG2_GLES=1 CONFIG2_AUDIO=1 CONFIG2_NVTT=0 CONFIG2_LOBBY=0
    CONFIG2_MINIUPNPC=0 CONFIG2_DAP_INTERFACE=0
    CONFIG2_FILE_ENABLE_AIO=0 _FILE_OFFSET_BITS=64
    U_STATIC_IMPLEMENTATION LIBICONV_STATIC
    BOOST_ALL_NO_LIB BOOST_SYSTEM_NO_LIB)
target_precompile_headers(pyrogenesis PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${SRC}/pch/engine/precompiled.h>")
target_compile_options(pyrogenesis PRIVATE -fvisibility=hidden -fno-omit-frame-pointer -fno-rtti
    -Wno-deprecated-declarations -Wno-inconsistent-missing-override)
target_link_options(pyrogenesis PRIVATE -Wl,--no-undefined -Wl,-z,max-page-size=16384
    -Wl,--gc-sections -Wl,--exclude-libs,ALL)

file(GLOB TINYGETTEXT_SOURCES "${SRC}/third_party/tinygettext/src/*.cpp")
add_library(tinygettext STATIC ${TINYGETTEXT_SOURCES})
get_target_property(ENGINE_INCLUDES pyrogenesis INCLUDE_DIRECTORIES)
target_include_directories(tinygettext PRIVATE ${ENGINE_INCLUDES})
target_compile_definitions(tinygettext PRIVATE NDEBUG CONFIG_FINAL=1 CONFIG_ENABLE_PCH=0)
add_library(mongoose STATIC "${SRC}/third_party/mongoose/mongoose.cpp")
add_library(glad STATIC "${SRC}/third_party/glad/src/gles2.cpp"
    "${SRC}/third_party/glad/src/vulkan.cpp")
target_include_directories(glad PUBLIC "${SRC}/third_party/glad/include")

target_link_directories(pyrogenesis PRIVATE "${PREFIX}/lib" "${PREFIX}/lib/static")
target_link_libraries(pyrogenesis PRIVATE tinygettext mongoose glad
    SDL2 mozjs-128
    "-Wl,--start-group" fmt enet xml2 png16 freetype openal vorbisfile vorbis ogg
    curl mbedtls mbedx509 mbedcrypto sodium iconv icui18n icuuc icudata
    "-Wl,--end-group" z log android OpenSLES GLESv2 EGL dl m)
