// SPDX-License-Identifier: GPL-2.0-or-later
package org.wildfiregames.zeroad;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

public class GameActivity extends SDLActivity {
    private boolean nativeLoaded;
    private static native void nativeSetPaths(String root, String cache);
    private static native void nativeTouchFrame(int action, int count, int firstId, int secondId,
            float x, float y, float secondX, float secondY, float density, long ageMs);
    private static native void nativeCancelTouches();

    @Override protected String[] getLibraries() {
        return new String[] {"c++_shared", "SDL2", "mozjs-128", "pyrogenesis"};
    }

    @Override protected String getMainSharedObject() {
        // Libraries are mapped directly from the APK; use the loaded SONAME.
        return "libpyrogenesis.so";
    }

    @Override protected String[] getArguments() {
        android.util.DisplayMetrics metrics = getResources().getDisplayMetrics();
        int width = metrics.widthPixels, height = metrics.heightPixels;
        if (mSurface instanceof GameSurface) {
            width = ((GameSurface) mSurface).renderWidth;
            height = ((GameSurface) mSurface).renderHeight;
        }
        float scale = PerformanceProfile.guiScale(width, height);
        return new String[] {"-mod=public", "-conf=rendererbackend:gl", "-conf=windowed:false",
            "-conf=gui.scale:" + scale, "-conf=forceglversion:true", "-conf=forceglprofile:es",
            "-conf=forceglmajorversion:2", "-conf=forceglminorversion:0"};
    }

    @Override protected SDLSurface createSDLSurface(Context context) {
        GameSurface surface = new GameSurface(context);
        // SDL's default RelativeLayout parameters are WRAP_CONTENT. A fixed
        // buffer can therefore shrink the View itself, leaving unused screen.
        surface.setLayoutParams(new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
        return surface;
    }

    @Override protected void onCreate(Bundle saved) {
        for (String library : getLibraries()) System.loadLibrary(library);
        nativeLoaded = true;
        nativeSetPaths(LauncherActivity.gameRoot(this).getAbsolutePath(), getCacheDir().getAbsolutePath());
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        // SDL posts a windowed style during startup. Reapply immersive mode
        // after that message as well as on resume/focus restoration.
        applyFullscreen();
        getWindow().getDecorView().post(this::applyFullscreen);
    }

    private void applyFullscreen() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(attributes);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override protected void onResume() {
        super.onResume();
        applyFullscreen();
    }

    @Override public void onBackPressed() {
        if (!nativeLoaded) return;
        nativeCancelTouches();
        onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE);
        onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE);
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus && nativeLoaded) nativeCancelTouches();
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyFullscreen();
    }

    @Override protected void onPause() {
        if (nativeLoaded) nativeCancelTouches();
        super.onPause();
    }

    @Override protected void onDestroy() {
        if (nativeLoaded) nativeCancelTouches();
        super.onDestroy();
        // Native engine singletons require a fresh process for the next session.
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    private static class GameSurface extends SDLSurface {
        private volatile int renderWidth = 1280, renderHeight = 720;

        GameSurface(Context context) {
            super(context);
            getHolder().setSizeFromLayout();
        }

        @Override protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
            super.onSizeChanged(width, height, oldWidth, oldHeight);
            if (width <= 0 || height <= 0) return;
            nativeCancelTouches();
        }

        @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            renderWidth = width;
            renderHeight = height;
            super.surfaceChanged(holder, format, width, height);
        }

        @Override public boolean onTouch(View view, MotionEvent event) {
            if ((event.getSource() & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE)
                return super.onTouch(view, event);
            if (event.getActionMasked() == MotionEvent.ACTION_CANCEL || getWidth() <= 0 || getHeight() <= 0) {
                nativeCancelTouches();
                return true;
            }
            int count = event.getPointerCount();
            nativeTouchFrame(event.getActionMasked(), count,
                    event.getPointerId(0), count > 1 ? event.getPointerId(1) : -1,
                    event.getX(0) / getWidth(), event.getY(0) / getHeight(),
                    count > 1 ? event.getX(1) / getWidth() : 0,
                    count > 1 ? event.getY(1) / getHeight() : 0,
                    getResources().getDisplayMetrics().density / getWidth(),
                    Math.max(0, SystemClock.uptimeMillis() - event.getEventTime()));
            return true;
        }
    }
}
