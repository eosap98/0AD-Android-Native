// SPDX-License-Identifier: GPL-2.0-or-later
package org.wildfiregames.zeroad;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Graphics defaults for the Adreno 618 class of phones. No Android API dependency. */
final class PerformanceProfile {
    private static final Pattern SECTION = Pattern.compile("^\\s*\\[([^]]*)]\\s*(?:;.*)?$");
    private static final Pattern SETTING = Pattern.compile("^\\s*([^=;\\[\\]]+?)\\s*=.*$");

    private PerformanceProfile() {}

    static float guiScale(int width, int height) {
        return Math.max(1f, Math.min(width / 1280f, height / 720f));
    }

    private static Map<String, String> settings() {
        Map<String, String> values = new LinkedHashMap<>();
        values.put("windowed", "false");
        values.put("xres", "0");
        values.put("yres", "0");
        values.put("vsync", "true");
        values.put("adaptivefps.session", "60");
        values.put("adaptivefps.menu", "60");
        // Scale only the 3D scene inside the renderer, never the Android View.
        values.put("android.scene.shortedge", "720");
        values.put("textures.quality", "1");
        values.put("textures.maxanisotropy", "1");
        values.put("materialmgr.quality", "3");
        values.put("max_actor_quality", "100");
        values.put("variant_diversity", "limited");
        values.put("smoothlos", "false");
        values.put("silhouettes", "false");
        values.put("shadows", "false");
        values.put("waterreflection", "false");
        values.put("waterrefraction", "false");
        values.put("waterrealdepth", "false");
        values.put("waterfancyeffects", "false");
        values.put("postproc", "false");
        values.put("particles", "false");
        return values;
    }

    static String updateConfig(String original) {
        Map<String, String> values = settings();
        Map<String, String> missing = new LinkedHashMap<>(values);
        String section = "";
        StringBuilder existing = new StringBuilder();
        for (String line : original.split("\\r?\\n")) {
            Matcher header = SECTION.matcher(line);
            Matcher setting = SETTING.matcher(line);
            if (header.matches()) {
                section = header.group(1) + ".";
            } else if (setting.matches()) {
                String name = setting.group(1).replaceAll("\\s", "");
                String key = section + name;
                // android.2 used this obsolete key, so textures stayed at high quality.
                if (key.equals("texturequality")) continue;
                if (values.containsKey(key)) {
                    line = name + " = \"" + values.get(key) + "\"";
                    missing.remove(key);
                }
            }
            existing.append(line).append('\n');
        }
        // New root keys must precede any existing [section] headers.
        StringBuilder result = new StringBuilder("; Android.4 performance defaults (editable in game)\n");
        for (Map.Entry<String, String> entry : missing.entrySet())
            result.append(entry.getKey()).append(" = \"").append(entry.getValue()).append("\"\n");
        return result.append(existing).toString();
    }

    static void install(File configDirectory) throws IOException {
        File config = new File(configDirectory, "user.cfg");
        File marker = new File(configDirectory, ".android-performance-4");
        if (marker.isFile() && config.isFile()) return;
        String original = "rendererbackend = \"gl\"\nwindowed = false\nsound.mastergain = 0.6\n";
        if (config.isFile()) {
            original = new String(Files.readAllBytes(config.toPath()), StandardCharsets.UTF_8);
            File backup = new File(configDirectory, "user.cfg.pre-android.4");
            if (!backup.exists()) Files.copy(config.toPath(), backup.toPath());
        }
        File temporary = new File(configDirectory, "user.cfg.android.4.tmp");
        try (FileOutputStream output = new FileOutputStream(temporary)) {
            output.write(updateConfig(original).getBytes(StandardCharsets.UTF_8));
            output.getFD().sync();
        }
        try {
            Files.move(temporary.toPath(), config.toPath(), StandardCopyOption.ATOMIC_MOVE,
                    StandardCopyOption.REPLACE_EXISTING);
        } catch (AtomicMoveNotSupportedException ignored) {
            Files.move(temporary.toPath(), config.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
        Files.write(marker.toPath(), "android.4\n".getBytes(StandardCharsets.UTF_8));
    }
}
