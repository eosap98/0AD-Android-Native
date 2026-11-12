#!/usr/bin/env python3
"""Fetch pinned dependency sources. Downloads/extraction are resumable."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('BUILD_ROOT', '/var/tmp/0ad-android-build'))
lock_path = root / 'dependencies.lock.json'
expected = json.loads(lock_path.read_text()) if lock_path.exists() else {}
sources = {
    'sdl': 'https://www.libsdl.org/release/SDL2-2.32.10.tar.gz',
    'enet': 'https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz',
    'fmt': 'https://github.com/fmtlib/fmt/archive/refs/tags/11.2.0.tar.gz',
    'png': 'https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.50.tar.gz',
    'freetype': 'https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz',
    'xml': 'https://download.gnome.org/sources/libxml2/2.13/libxml2-2.13.8.tar.xz',
    'ogg': 'https://github.com/xiph/ogg/archive/refs/tags/v1.3.5.tar.gz',
    'vorbis': 'https://github.com/xiph/vorbis/archive/refs/tags/v1.3.7.tar.gz',
    'openal': 'https://github.com/kcat/openal-soft/archive/refs/tags/1.24.3.tar.gz',
    'mbedtls': 'https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v2.28.10.tar.gz',
    'curl': 'https://curl.se/download/curl-8.14.1.tar.xz',
    'sodium': 'https://download.libsodium.org/libsodium/releases/libsodium-1.0.20.tar.gz',
    'icu': 'https://github.com/unicode-org/icu/releases/download/release-76-1/icu4c-76_1-src.tgz',
    'iconv': 'https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.18.tar.gz',
    'boost': 'https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.bz2',
}

def fetch(item):
    name, url = item
    archive = work / 'downloads' / (name + '.tar')
    archive.parent.mkdir(parents=True, exist_ok=True)
    dest = work / 'src' / name
    if not archive.exists():
        part = archive.with_suffix('.part')
        subprocess.run(['curl', '-fL', '--retry', '3', '--connect-timeout', '30',
                        '-o', str(part), url], check=True, stdout=subprocess.DEVNULL)
        part.rename(archive)
    digest = hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest()
    if name in expected and expected[name]['url'] == url and expected[name]['sha256'] != digest:
        raise RuntimeError('Source checksum mismatch: ' + name)
    if not (dest / '.extracted').exists():
        dest.mkdir(parents=True, exist_ok=True)
        args = ['tar', 'xf', str(archive), '--strip-components=1', '-C', str(dest)]
        if name == 'boost':
            args += ['--wildcards', '*/boost/*', '*/LICENSE_1_0.txt']
        subprocess.run(args, check=True)
        (dest / '.extracted').touch()
    print(name, digest, flush=True)
    return name, {'url': url, 'sha256': digest}

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    lock = dict(pool.map(fetch, sources.items()))
(root / 'dependencies.lock.json').write_text(json.dumps(lock, indent=2) + '\n')
