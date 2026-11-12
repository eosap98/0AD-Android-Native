#!/usr/bin/env python3
"""Check the Linux host and pinned Android toolchain without building anything."""
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

errors = []

def check(condition, message):
    print(('OK   ' if condition else 'FAIL ') + message, flush=True)
    if not condition:
        errors.append(message)

def version(command):
    try:
        result = subprocess.run(command, text=True, capture_output=True, timeout=30)
        output = (result.stdout + result.stderr).strip()
        if result.returncode:
            return ''
        return output
    except (OSError, subprocess.TimeoutExpired):
        return ''

check(platform.system() == 'Linux' and platform.machine() == 'x86_64',
      'Host Linux x86_64 (Windows: gunakan Ubuntu di WSL2)')
check(sys.version_info >= (3, 12), 'Python >= 3.12')
for name in ('bash', 'curl', 'tar', 'xz', 'unzip', 'patch', 'make', 'pkg-config',
             'cc', 'c++', 'clang', 'clang++', 'autoconf', 'perl', 'm4', 'ninja', 'git'):
    check(shutil.which(name) is not None, 'Tool host: ' + name)

work = Path(os.environ['BUILD_ROOT'])
check(work.is_absolute() and not any(c.isspace() for c in str(work)),
      'BUILD_ROOT absolut dan tanpa spasi: ' + str(work))
check(os.environ.get('JOBS', '').isdigit() and int(os.environ['JOBS']) > 0,
      'JOBS berupa angka positif')
cmake = version([os.environ['CMAKE'], '--version'])
match = re.search(r'cmake version (\d+)\.(\d+)\.(\d+)', cmake)
check(bool(match and (3, 26, 0) <= tuple(map(int, match.groups())) < (4, 0, 0)),
      'CMake >= 3.26 dan < 4 (paket ini menggunakan 3.31.6)')
gradle = Path(os.environ['GRADLE'])
check(gradle.is_file() and os.access(gradle, os.X_OK), 'Gradle: ' + str(gradle))
if gradle.is_file() and os.access(gradle, os.X_OK):
    check(bool(re.search(r'\bGradle 8\.9\b', version([str(gradle), '--version']))), 'Gradle 8.9')
javac = version([str(Path(os.environ['JAVA_HOME']) / 'bin/javac'), '-version'])
check(bool(re.search(r'javac (17|21)(?:\.|\b)', javac)), 'JDK 17 atau 21 (disarankan 21)')
check('rustc 1.84.1 ' in version(['rustc', '--version']), 'Rust 1.84.1')
check('cargo 1.84.1 ' in version(['cargo', '--version']), 'Cargo 1.84.1')
rustlib = version(['rustc', '--print', 'target-libdir', '--target', 'aarch64-linux-android'])
check(bool(rustlib and Path(rustlib).is_dir() and list(Path(rustlib).glob('libstd-*.rlib'))),
      'Rust target aarch64-linux-android')
cbindgen = version(['cbindgen', '--version'])
match = re.search(r'cbindgen (\d+)\.(\d+)\.(\d+)', cbindgen)
check(bool(match and tuple(map(int, match.groups())) >= (0, 26, 0)), 'cbindgen >= 0.26 (disarankan 0.29.2)')

sdk = Path(os.environ['ANDROID_SDK_ROOT'])
ndk = Path(os.environ['ANDROID_NDK_ROOT'])
for rel in ('platforms/android-36/android.jar', 'build-tools/36.0.0/aapt2',
            'build-tools/36.0.0/apksigner', 'build-tools/36.0.0/zipalign'):
    check((sdk / rel).is_file(), 'Android SDK: ' + rel)
ndk_props = ndk / 'source.properties'
check(ndk_props.is_file() and bool(re.search(r'Pkg.Revision\s*=\s*27\.2\.12479018\s*$',
      ndk_props.read_text() if ndk_props.is_file() else '', re.M)), 'Android NDK 27.2.12479018')
check((ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang').is_file(),
      'Compiler NDK ARM64 / API 29')
if errors:
    print('\nLengkapi prasyarat di BUILD-PC-BARU.md. Build belum dijalankan.', file=sys.stderr)
    raise SystemExit(1)
print('\nLingkungan build siap. Source/data diperiksa saat build dimulai.')
