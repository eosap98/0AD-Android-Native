#!/usr/bin/env python3
from pathlib import Path
import os
import shutil

port = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('BUILD_ROOT', '/var/tmp/0ad-android-build'))
game = port.parent / '0ad-0.28.0-unix-build/0ad-0.28.0'
out = work / 'assets/licenses'
out.mkdir(parents=True, exist_ok=True)
for name in ('LICENSE.md', 'license_gpl-2.0.txt', 'license_lgpl-2.1.txt', 'license_mit.txt'):
    shutil.copyfile(game / name, out / ('0ad-' + name))
for name in ('sdl', 'enet', 'fmt', 'png', 'freetype', 'xml', 'ogg', 'vorbis',
             'openal', 'mbedtls', 'curl', 'sodium', 'icu', 'iconv', 'boost', 'mozjs-128.13.0'):
    src = work / 'src' / name
    candidates = list(src.glob('LICENSE*')) + list(src.glob('COPYING*')) + list(src.glob('Copyright*'))
    if not candidates:
        raise RuntimeError('No license found for ' + name)
    for file in candidates:
        if file.is_file(): shutil.copyfile(file, out / (name + '-' + file.name))
for file in (work / 'src/freetype/docs').glob('*.TXT'):
    if file.name in ('FTL.TXT', 'GPLv2.TXT'):
        shutil.copyfile(file, out / ('freetype-' + file.name))
shutil.copyfile(work / 'src/openal/LICENSE-pffft', out / 'openal-LICENSE-pffft')
shutil.copyfile(work / 'src/openal/fmt-11.1.1/LICENSE', out / 'openal-fmt-LICENSE')
shutil.copyfile(port / 'dependencies.lock.json', work / 'assets/dependencies.lock.json')
(out / 'NOTICE.txt').write_text('''0 A.D. / Pyrogenesis 0.28.0 — Copyright Wildfire Games and contributors.
This is an experimental community Android port, not an official Android release.
Game artwork and audio attribution/license files are retained in the data archives.
Engine source: the supplied 0ad-0.28.0-unix-build source distribution, with the
Android modifications and build scripts delivered in android-port/ alongside it.
SDL2, SpiderMonkey and other dependencies are built from the pinned source
archives listed in dependencies.lock.json. Their licenses are included here.
Android NDK libc++ uses Apache-2.0 with LLVM exceptions; see libcxx-LICENSE.txt.
''')
ndk = Path(os.environ.get('ANDROID_NDK_ROOT', '/home/server/Android/Sdk/ndk/27.2.12479018'))
license_path = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/share/licenses/libc++/LICENSE.TXT'
if license_path.exists(): shutil.copyfile(license_path, out / 'libcxx-LICENSE.txt')
else: shutil.copyfile(ndk / 'NOTICE.toolchain', out / 'libcxx-LICENSE.txt')
