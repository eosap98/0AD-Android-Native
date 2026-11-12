#!/usr/bin/env python3
"""Restore pinned upstream archives and apply the current Android engine patch."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

root = Path(__file__).resolve().parents[1]
lock = json.loads((root / 'upstream.lock.json').read_text())
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--engine-only', action='store_true', help='Siapkan source tanpa mengunduh data game')
args = parser.parse_args()

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def restore(kind, required):
    entry = lock[kind]
    parent = root / entry['directory']
    target = parent / ('0ad-' + lock['version'])
    if target.exists():
        missing = [name for name in required if not (target / name).is_file()]
        if missing:
            raise SystemExit('Folder belum lengkap: ' + str(target) + '\nBerkas kurang: ' + ', '.join(missing))
        print('Menggunakan folder yang tersedia:', target, flush=True)
        return target
    archive = root / entry['filename']
    if not archive.exists():
        part = archive.with_name(archive.name + '.part')
        print('Mengunduh:', entry['url'], flush=True)
        subprocess.run(['curl', '-fL', '--retry', '3', '--connect-timeout', '30',
                        '-o', str(part), entry['url']], check=True)
        if digest(part) != entry['sha256']:
            raise SystemExit('Checksum unduhan tidak cocok: ' + str(part))
        part.rename(archive)
    elif digest(archive) != entry['sha256']:
        raise SystemExit('Checksum arsip tidak cocok: ' + str(archive))
    print('Checksum cocok; mengekstrak', archive.name, flush=True)
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.extract-', dir=parent) as temp:
        with tarfile.open(archive, 'r:xz') as package:
            package.extractall(temp, filter='data')
        extracted = Path(temp) / target.name
        if not all((extracted / name).is_file() for name in required):
            raise SystemExit('Struktur arsip tidak sesuai: ' + archive.name)
        extracted.rename(target)
    return target

source = restore('source', ['source/main.cpp', 'source/renderer/Renderer.cpp',
    'libraries/source/spidermonkey/mozjs-128.13.0.tar.xz',
    'libraries/source/cxxtest-4.4/cxxtest-4.4.tar.gz', 'LICENSE.md'])
patch = root / 'android-port/patches/engine-android.patch'
command = ['patch', '--batch', '-p1', '-d', str(source), '-i', str(patch)]
reverse = subprocess.run(command + ['--dry-run', '--reverse', '--force'], capture_output=True, text=True)
if reverse.returncode == 0:
    print('Patch Android terbaru sudah terpasang.', flush=True)
else:
    forward = subprocess.run(command + ['--dry-run', '--forward'], capture_output=True, text=True)
    if forward.returncode:
        raise SystemExit('Source berbeda dari release/patch yang diharapkan. Folder tidak ditimpa.\n' + forward.stdout + forward.stderr)
    subprocess.run(command + ['--forward'], check=True)
    print('Patch Android diterapkan.', flush=True)
if not args.engine_only:
    restore('data', ['binaries/data/config/default.cfg', 'binaries/data/config/keys.txt',
        'binaries/data/mods/mod/mod.zip', 'binaries/data/mods/public/public.zip',
        'binaries/data/mods/public/mod.json'])
print('Persiapan source selesai.', flush=True)
