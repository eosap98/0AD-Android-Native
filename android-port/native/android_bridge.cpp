// SPDX-License-Identifier: GPL-2.0-or-later
#include <SDL.h>
#include <jni.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <unistd.h>

extern "C" int pyrogenesis_main(int argc, char* argv[]);

extern "C" JNIEXPORT void JNICALL
Java_org_wildfiregames_zeroad_GameActivity_nativeSetPaths(
    JNIEnv* env, jclass, jstring root, jstring cache)
{
    const char* rootChars = env->GetStringUTFChars(root, nullptr);
    const char* cacheChars = env->GetStringUTFChars(cache, nullptr);
    setenv("ZEROAD_ROOT", rootChars, 1);
    setenv("ZEROAD_CACHE", cacheChars, 1);
    const std::string log = std::string(rootChars) + "/logs/native.log";
    freopen(log.c_str(), "a", stdout);
    freopen(log.c_str(), "a", stderr);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    env->ReleaseStringUTFChars(root, rootChars);
    env->ReleaseStringUTFChars(cache, cacheChars);
}

extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char* argv[])
{
	// SDL otherwise changes a resizable window to fullUser orientation,
	// overriding the activity's landscape setting during engine startup.
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    // Complete MotionEvent frames are translated on the engine thread.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
    return pyrogenesis_main(argc, argv);
}
