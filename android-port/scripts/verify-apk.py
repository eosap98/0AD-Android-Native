#!/usr/bin/env python3
"""Verify APK contents, ABI, native dependency closure and data identity."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile

apk = Path(sys.argv[1]).resolve()
port = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('BUILD_ROOT', '/var/tmp/0ad-android-build'))
ndk = Path(os.environ.get('ANDROID_NDK_ROOT', '/home/server/Android/Sdk/ndk/27.2.12479018'))
readelf = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
system_dir = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/29'
# Accept only libraries publicly provided by our actual minimum Android API.
# SDL also links GLES 1 compatibility support, even when the engine uses GLES 2.
system = {p.name for p in system_dir.glob('*.so')}
assert 'libc.so' in system, 'Android API 29 system stubs are missing'
report = {'apk': apk.name, 'size': apk.stat().st_size, 'libraries': {},
          'game_data_verified': [], 'physical_device_tested': False}
with zipfile.ZipFile(apk) as z, tempfile.TemporaryDirectory(dir=work) as temp:
    libraries = [n for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')]
    assert libraries and all(n.startswith('lib/arm64-v8a/') for n in libraries)
    names = {Path(n).name for n in libraries}
    assert {'libpyrogenesis.so', 'libSDL2.so', 'libmozjs-128.so', 'libc++_shared.so'} <= names
    for name in libraries:
        binary = z.read(name)
        assert binary[:5] == b'\x7fELF\x02', name
        assert struct.unpack_from('<H', binary, 18)[0] == 183, name  # EM_AARCH64
        path = Path(temp) / Path(name).name
        path.write_bytes(binary)
        dynamic = subprocess.check_output([readelf, '-d', path], text=True)
        needed = [line.split('[')[1].split(']')[0] for line in dynamic.splitlines() if '(NEEDED)' in line]
        assert all(lib in names | system for lib in needed), (name, needed)
        segments = subprocess.check_output([readelf, '-l', path], text=True)
        assert all(int(line.split()[-1], 16) >= 16384 for line in segments.splitlines() if line.strip().startswith('LOAD')), name
        if name.endswith('/libpyrogenesis.so'):
            symbols = subprocess.check_output([readelf, '--dyn-syms', path], text=True)
            assert ' SDL_main' in symbols and ' pyrogenesis_main' in symbols
            assert 'Java_org_wildfiregames_zeroad_GameActivity_nativeSetPaths' in symbols
            assert 'Java_org_wildfiregames_zeroad_GameActivity_nativeTouchFrame' in symbols
            assert 'Java_org_wildfiregames_zeroad_GameActivity_nativeCancelTouches' in symbols
            with (work / 'jniLibs/arm64-v8a/libpyrogenesis.so').open('rb') as built:
                assert hashlib.sha256(binary).hexdigest() == hashlib.file_digest(built, 'sha256').hexdigest(), 'Stale engine library'
        report['libraries'][Path(name).name] = {'size': len(binary), 'needed': needed,
            'sha256': hashlib.sha256(binary).hexdigest(), 'elf_alignment': 16384}
    for entry in json.loads(z.read('assets/data-manifest.json')):
        name = 'assets/data/' + entry['path']
        assert z.getinfo(name).file_size == entry['size'], name
        with z.open(name) as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        assert digest == entry['sha256'], name
        report['game_data_verified'].append(entry['path'])
    assert z.getinfo('assets/data/mods/public/public.zip').compress_type == zipfile.ZIP_STORED
    assert 'classes.dex' in z.namelist() and 'AndroidManifest.xml' in z.namelist()
    dex = z.read('classes.dex')
    assert all(symbol in dex for symbol in (b'nativeTouchFrame', b'nativeCancelTouches', b'LauncherScene;', b'PerformanceProfile;'))
sdk = Path(os.environ.get('ANDROID_SDK_ROOT', '/home/server/Android/Sdk'))
badging = subprocess.check_output([sdk / 'build-tools/36.0.0/aapt2', 'dump', 'badging', apk], text=True)
assert "name='org.wildfiregames.zeroad'" in badging
assert "versionCode='280004'" in badging and "versionName='0.28.0-android.4'" in badging
assert "minSdkVersion:'29'" in badging and "targetSdkVersion:'35'" in badging
assert "native-code: 'arm64-v8a'" in badging
assert "launchable-activity: name='org.wildfiregames.zeroad.LauncherActivity'" in badging
report['version'] = '0.28.0-android.4'
report['version_code'] = 280004
report['min_sdk'] = 29
report['target_sdk'] = 35
report['gesture_jni_verified'] = True
report['sha256'] = hashlib.file_digest(apk.open('rb'), 'sha256').hexdigest()
(port / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
print('APK verified:', apk.name, len(report['libraries']), 'ARM64 libraries,',
      len(report['game_data_verified']), 'data files. Physical-device runtime not tested.')
