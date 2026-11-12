// SPDX-License-Identifier: GPL-2.0-or-later
package org.wildfiregames.zeroad;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Map;
import java.util.stream.Stream;

public final class PerformanceProfileTest {
    private static int passed;

    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static void pass(String name) {
        ++passed;
        System.out.println("PASS " + name);
    }

    // Read the effective values with 0 A.D.'s section-prefix and last-value rules.
    private static Map<String, String> values(String config) {
        Map<String, String> result = new HashMap<>();
        String prefix = "";
        for (String line : config.split("\n")) {
            line = line.trim();
            if (line.startsWith("[")) prefix = line.substring(1, line.indexOf(']')) + ".";
            else if (!line.startsWith(";") && line.contains("=")) {
                int equal = line.indexOf('=');
                result.put(prefix + line.substring(0, equal).trim(),
                        line.substring(equal + 1).trim().replace("\"", ""));
            }
        }
        return result;
    }

    public static void main(String[] args) throws Exception {
        check(PerformanceProfile.guiScale(2400, 1080) == 1.5f, "GUI must use the full Redmi display");
        check(2400 / PerformanceProfile.guiScale(2400, 1080) == 1600, "Logical HUD width changed");
        pass("Redmi Note 9 Pro keeps its original logical HUD size on the full display");

        check(PerformanceProfile.guiScale(1920, 1080) == 1.5f, "16:9 HUD scale");
        check(PerformanceProfile.guiScale(960, 540) == 1f, "Small-window HUD scale");
        pass("HUD scaling remains valid on other display sizes");

        String original = "; keep this comment\ntexturequality = 1\ntextures.quality = 2\n" +
                "sound.mastergain = 0.27\nplayername = \"Player\"\nhotkey.cancel = \"Escape\"\n";
        String updated = PerformanceProfile.updateConfig(original);
        Map<String, String> config = values(updated);
        check(config.get("textures.quality").equals("1") && !config.containsKey("texturequality"), "Texture migration");
        check(config.get("adaptivefps.session").equals("60") && config.get("adaptivefps.menu").equals("60"), "FPS defaults");
        check(config.get("vsync").equals("true"), "VSync disabled");
        check(config.get("sound.mastergain").equals("0.27") && config.get("playername").equals("Player") &&
                config.get("hotkey.cancel").equals("Escape") && updated.contains("; keep this comment"), "Unrelated setting changed");
        pass("upgrade repairs the texture key and preserves unrelated preferences");

        config = values(PerformanceProfile.updateConfig("[adaptivefps]\nsession = 60\nmenu = 120\n" +
                "[textures]\nquality = 2\nquality = 0\n[hotkey]\ncancel = Escape\n"));
        check(config.get("adaptivefps.session").equals("60") && config.get("adaptivefps.menu").equals("60"), "Section FPS keys");
        check(config.get("textures.quality").equals("1") && config.get("hotkey.cancel").equals("Escape"), "Section texture keys");
        check(config.get("smoothlos").equals("false") && !config.containsKey("hotkey.smoothlos"), "New setting inherited a section");
        pass("sectioned configs and repeated keys receive valid defaults");

        config = values(PerformanceProfile.updateConfig("windowed = true\nxres = 1600\nyres = 720\nadaptivefps.session = 30\n"));
        check(config.get("windowed").equals("false") && config.get("xres").equals("0") && config.get("yres").equals("0"), "Stale display size");
        check(config.get("android.scene.shortedge").equals("720") && config.get("adaptivefps.session").equals("60"), "Scene/FPS defaults");
        check(config.get("silhouettes").equals("false") && config.get("variant_diversity").equals("limited"), "Expensive model passes");
        pass("upgrade clears old window dimensions and sets the 60 FPS scene profile");

        Path directory = Files.createTempDirectory("zeroad-profile-test-");
        try {
            PerformanceProfile.install(directory.toFile());
            check(values(Files.readString(directory.resolve("user.cfg"))).get("sound.mastergain").equals("0.6"), "Fresh install");
            pass("fresh installations receive graphics and audio defaults");

            Files.delete(directory.resolve(".android-performance-4"));
            Files.writeString(directory.resolve(".android-performance-3"), "android.3\n");
            Files.writeString(directory.resolve("user.cfg"), original);
            PerformanceProfile.install(directory.toFile());
            check(Files.readString(directory.resolve("user.cfg.pre-android.4")).equals(original), "Backup differs");
            check(!Files.exists(directory.resolve("user.cfg.android.4.tmp")), "Uncommitted configuration");
            check(Files.exists(directory.resolve(".android-performance-4")), "android.3 marker skipped migration");
            pass("android.3 installations receive the new profile with an original-config backup");

            String customized = "adaptivefps.session = 45\nsound.mastergain = 0.2\n";
            Files.writeString(directory.resolve("user.cfg"), customized);
            PerformanceProfile.install(directory.toFile());
            check(Files.readString(directory.resolve("user.cfg")).equals(customized), "Later user edits overwritten");
            pass("later launches retain the user's graphics changes");

            Files.delete(directory.resolve("user.cfg"));
            PerformanceProfile.install(directory.toFile());
            check(values(Files.readString(directory.resolve("user.cfg"))).get("adaptivefps.session").equals("60"), "Config not recovered");
            check(Files.readString(directory.resolve("user.cfg.pre-android.4")).equals(original), "Original backup overwritten");
            pass("resetting preferences restores defaults while preserving the original backup");
        } finally {
            try (Stream<Path> paths = Files.walk(directory)) {
                for (Path path : paths.sorted(Comparator.reverseOrder()).toArray(Path[]::new)) Files.delete(path);
            }
        }
        System.out.println(passed + " performance-profile checks passed; no Android emulator used.");
    }
}
