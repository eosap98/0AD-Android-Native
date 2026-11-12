// SPDX-License-Identifier: GPL-2.0-or-later
package org.wildfiregames.zeroad;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Bundle;
import android.os.StatFs;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.Locale;

public class LauncherActivity extends Activity {
    private static final int GOLD = 0xFFEDBD68, TEXT = 0xFFF0F3F7, MUTED = 0xFF9CAFC2;
    private TextView status;
    private Button play;
    private ProgressBar progress;
    private boolean preparing, ready;
    private volatile boolean destroyed;
    private Thread installer;

    public static File gameRoot(Context context) {
        File storage = context.getExternalFilesDir(null);
        return new File(storage != null ? storage : context.getFilesDir(), "0ad");
    }

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        immersive();

        LinearLayout hero = column();
        TextView eyebrow = text(R.string.launcher_eyebrow, 10, MUTED);
        eyebrow.setLetterSpacing(0.22f);
        hero.addView(eyebrow);
        TextView title = text(R.string.launcher_title, 62, TEXT);
        title.setTypeface(Typeface.create("serif", Typeface.BOLD));
        title.setLetterSpacing(0.04f);
        title.setMaxLines(1);
        title.setAutoSizeTextTypeUniformWithConfiguration(32, 62, 1, TypedValue.COMPLEX_UNIT_SP);
        hero.addView(title, new LinearLayout.LayoutParams(-1, dp(70)));
        TextView subtitle = text(R.string.launcher_subtitle, 11, GOLD);
        subtitle.setLetterSpacing(0.20f);
        hero.addView(subtitle);
        TextView tagline = text(R.string.launcher_tagline, 12, MUTED);
        LinearLayout.LayoutParams taglineParams = new LinearLayout.LayoutParams(-1, -2);
        taglineParams.topMargin = dp(9);
        hero.addView(tagline, taglineParams);

        LinearLayout actions = column();
        play = new Button(this);
        play.setText(R.string.play);
        play.setTextSize(14);
        play.setLetterSpacing(0.10f);
        play.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        play.setTextColor(0xFF141D29);
        play.setBackground(ripple(GOLD, 14));
        play.setElevation(dp(4));
        play.setStateListAnimator(null);
        play.setOnClickListener(v -> {
            if (ready) startActivity(new Intent(this, GameActivity.class));
            else beginPreparation();
        });
        actions.addView(play, new LinearLayout.LayoutParams(-1, dp(52)));
        status = text(R.string.checking, 12, MUTED);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(-1, -2);
        statusParams.topMargin = dp(14);
        actions.addView(status, statusParams);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setProgressTintList(ColorStateList.valueOf(GOLD));
        progress.setIndeterminateTintList(ColorStateList.valueOf(GOLD));
        progress.setProgressBackgroundTintList(ColorStateList.valueOf(0xFF253343));
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(-1, dp(3));
        progressParams.setMargins(dp(24), dp(10), dp(24), 0);
        actions.addView(progress, progressParams);

        LinearLayout footer = column();
        LinearLayout links = new LinearLayout(this);
        links.setBaselineAligned(false);
        links.setGravity(Gravity.CENTER);
        TextView help = footerLink(R.string.controls);
        help.setOnClickListener(v -> new AlertDialog.Builder(this)
                .setTitle(R.string.controls_title).setMessage(R.string.controls_message)
                .setPositiveButton(R.string.understood, null).show());
        links.addView(help, new LinearLayout.LayoutParams(0, -2, 1));
        View divider = new View(this);
        divider.setBackgroundColor(0xFF314052);
        links.addView(divider, new LinearLayout.LayoutParams(dp(1), dp(24)));
        TextView about = footerLink(R.string.about);
        about.setOnClickListener(v -> new AlertDialog.Builder(this)
                .setTitle(R.string.about).setMessage(R.string.about_message)
                .setPositiveButton(R.string.understood, null).show());
        links.addView(about, new LinearLayout.LayoutParams(0, -2, 1));
        footer.addView(links, new LinearLayout.LayoutParams(-1, -2));
        footer.addView(text(R.string.launcher_version, 10, MUTED));

        LauncherScene scene = new LauncherScene(this, hero, actions, footer);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setOverScrollMode(View.OVER_SCROLL_NEVER);
        scroll.setBackgroundColor(0xFF0B121C);
        scroll.addView(scene, new ScrollView.LayoutParams(-1, -2));
        scroll.setOnApplyWindowInsetsListener((view, insets) -> {
            android.view.DisplayCutout cutout = insets.getDisplayCutout();
            view.setPadding(cutout == null ? 0 : cutout.getSafeInsetLeft(),
                    cutout == null ? 0 : cutout.getSafeInsetTop(),
                    cutout == null ? 0 : cutout.getSafeInsetRight(),
                    cutout == null ? 0 : cutout.getSafeInsetBottom());
            return insets;
        });
        setContentView(scroll);
        beginPreparation();
    }

    private void immersive() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) immersive();
    }

    private int dp(float value) { return Math.round(value * getResources().getDisplayMetrics().density); }

    private LinearLayout column() {
        LinearLayout view = new LinearLayout(this);
        view.setOrientation(LinearLayout.VERTICAL);
        view.setGravity(Gravity.CENTER);
        return view;
    }

    private TextView text(int resource, int size, int color) {
        TextView view = new TextView(this);
        view.setText(resource);
        view.setTextSize(size);
        view.setTextColor(color);
        view.setGravity(Gravity.CENTER);
        view.setIncludeFontPadding(false);
        return view;
    }

    private TextView footerLink(int resource) {
        TextView view = text(resource, 12, MUTED);
        view.setMinHeight(dp(48));
        view.setPadding(dp(12), dp(8), dp(12), dp(8));
        view.setBackground(ripple(0x00000000, 12));
        view.setFocusable(true);
        return view;
    }

    private RippleDrawable ripple(int color, int radius) {
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(color);
        shape.setCornerRadius(dp(radius));
        GradientDrawable mask = new GradientDrawable();
        mask.setColor(0xFFFFFFFF);
        mask.setCornerRadius(dp(radius));
        return new RippleDrawable(ColorStateList.valueOf(0x30FFFFFF), shape, mask);
    }

    private void enablePlay(boolean enabled) {
        play.setEnabled(enabled);
        play.setAlpha(enabled ? 1f : 0.45f);
    }

    private void beginPreparation() {
        if (preparing || destroyed) return;
        preparing = true;
        ready = false;
        enablePlay(false);
        play.setText(R.string.play);
        status.setText(R.string.checking);
        status.setTextColor(MUTED);
        progress.setVisibility(View.VISIBLE);
        progress.setIndeterminate(true);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        installer = new Thread(() -> {
            try {
                prepareData();
                updateUi(() -> {
                    preparing = false;
                    ready = true;
                    enablePlay(true);
                    progress.setIndeterminate(false);
                    progress.setProgress(1000);
                    progress.setVisibility(View.INVISIBLE);
                    status.setText(R.string.ready);
                    status.setTextColor(0xFF94D5BA);
                    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                });
            } catch (Exception error) {
                if (!destroyed) android.util.Log.e("0ad", "Data preparation failed", error);
                updateUi(() -> {
                    preparing = false;
                    ready = false;
                    enablePlay(true);
                    play.setText(R.string.retry);
                    progress.setIndeterminate(false);
                    progress.setVisibility(View.INVISIBLE);
                    status.setText(getString(R.string.preparation_error, error.getMessage()));
                    status.setTextColor(0xFFFFAAA0);
                    getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                });
            }
        }, "PrepareGameData");
        installer.start();
    }

    private void updateUi(Runnable action) {
        if (!destroyed) runOnUiThread(() -> { if (!destroyed) action.run(); });
    }

    private void checkCancelled() throws InterruptedIOException {
        if (destroyed || Thread.currentThread().isInterrupted())
            throw new InterruptedIOException("Persiapan dihentikan");
    }

    private void prepareData() throws Exception {
        File root = gameRoot(this);
        mkdir(root);
        byte[] manifest;
        try (InputStream in = getAssets().open("data-manifest.json")) {
            manifest = readStream(in);
        }
        JSONArray files = new JSONArray(new String(manifest, StandardCharsets.UTF_8));
        String manifestHash = hex(MessageDigest.getInstance("SHA-256").digest(manifest));
        File marker = new File(root, ".data-ready");
        boolean ready = marker.isFile() && new String(Files.readAllBytes(marker.toPath()), StandardCharsets.UTF_8).equals(manifestHash);
        long total = 0;
        for (int i = 0; i < files.length(); i++) {
            checkCancelled();
            JSONObject entry = files.getJSONObject(i);
            total += entry.getLong("size");
            File file = new File(root, "data/" + entry.getString("path"));
            ready &= file.isFile() && file.length() == entry.getLong("size");
        }
        if (!ready) {
            boolean[] verified = new boolean[files.length()];
            long required = 0;
            for (int i = 0; i < files.length(); i++) {
                checkCancelled();
                JSONObject entry = files.getJSONObject(i);
                File file = new File(root, "data/" + entry.getString("path"));
                // Incomplete copies belong to this installer and can be retried.
                File partial = new File(file + ".part");
                if (partial.exists() && !partial.delete()) throw new IOException("Tidak dapat menghapus salinan data yang terputus");
                if (file.isFile() && file.length() == entry.getLong("size")) {
                    MessageDigest digest = MessageDigest.getInstance("SHA-256");
                    try (InputStream in = new FileInputStream(file)) {
                        byte[] bytes = new byte[1024 * 1024];
                        int count;
                        while ((count = in.read(bytes)) != -1) {
                            checkCancelled();
                            digest.update(bytes, 0, count);
                        }
                    }
                    verified[i] = hex(digest.digest()).equals(entry.getString("sha256"));
                }
                if (!verified[i]) required += entry.getLong("size");
            }
            if (new StatFs(root.getPath()).getAvailableBytes() < required + 128L * 1024 * 1024)
                throw new IOException("Diperlukan sedikitnya 2,1 GB ruang kosong untuk menyiapkan data");
            long done = 0;
            byte[] buffer = new byte[1024 * 1024];
            for (int i = 0; i < files.length(); i++) {
                checkCancelled();
                JSONObject entry = files.getJSONObject(i);
                if (verified[i]) { done += entry.getLong("size"); continue; }
                String relative = entry.getString("path");
                File destination = new File(root, "data/" + relative);
                mkdir(destination.getParentFile());
                File partial = new File(destination + ".part");
                MessageDigest digest = MessageDigest.getInstance("SHA-256");
                long copied = 0, lastUpdate = 0;
                try (InputStream in = getAssets().open("data/" + relative);
                     FileOutputStream out = new FileOutputStream(partial)) {
                    int count;
                    while ((count = in.read(buffer)) != -1) {
                        checkCancelled();
                        out.write(buffer, 0, count);
                        digest.update(buffer, 0, count);
                        copied += count;
                        long now = android.os.SystemClock.elapsedRealtime();
                        if (now - lastUpdate > 200) {
                            report(done + copied, total);
                            lastUpdate = now;
                        }
                    }
                    out.getFD().sync();
                }
                if (copied != entry.getLong("size") || !hex(digest.digest()).equals(entry.getString("sha256"))) {
                    partial.delete();
                    throw new IOException("Verifikasi data gagal: " + relative);
                }
                Files.move(partial.toPath(), destination.toPath(), StandardCopyOption.REPLACE_EXISTING);
                done += copied;
            }
            Files.write(marker.toPath(), manifestHash.getBytes(StandardCharsets.UTF_8));
        }
        mkdir(new File(root, "logs"));
        mkdir(new File(root, "config"));
        PerformanceProfile.install(new File(root, "config"));
        report(total, total);
    }

    private void report(long done, long total) {
        updateUi(() -> {
            progress.setIndeterminate(false);
            progress.setProgress(total == 0 ? 1000 : (int) (done * 1000 / total));
            status.setText(getString(R.string.progress, total == 0 ? 100 : (int) (done * 100 / total)));
        });
    }

    private static void mkdir(File path) throws IOException {
        if (!path.isDirectory() && !path.mkdirs()) throw new IOException("Tidak dapat membuat " + path);
    }
    private static byte[] readStream(InputStream in) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] bytes = new byte[8192];
        int count;
        while ((count = in.read(bytes)) != -1) out.write(bytes, 0, count);
        return out.toByteArray();
    }
    private static String hex(byte[] bytes) {
        StringBuilder out = new StringBuilder();
        for (byte b : bytes) out.append(String.format(Locale.ROOT, "%02x", b & 255));
        return out.toString();
    }
    @Override protected void onDestroy() {
        destroyed = true;
        if (installer != null) installer.interrupt();
        super.onDestroy();
    }
}
