#!/usr/bin/env python3
"""Recompress the supplied store-only archives without changing their contents."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('BUILD_ROOT', '/var/tmp/0ad-android-build'))
data = root.parent / '0ad-0.28.0-unix-data/0ad-0.28.0/binaries/data'
assets = work / 'assets/data'
assets.mkdir(parents=True, exist_ok=True)
manifest = []
for rel in ('mods/mod/mod.zip', 'mods/public/public.zip'):
    src, dest = data / rel, assets / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    if not dest.exists():
        part = dest.with_suffix('.part')
        with zipfile.ZipFile(src) as zin, zipfile.ZipFile(part, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as zout:
            for i, info in enumerate(zin.infolist()):
                with zin.open(info) as fi, zout.open(info.filename, 'w') as fo:
                    shutil.copyfileobj(fi, fo, 1024 * 1024)
                if i % 5000 == 0:
                    print(rel, i, '/', len(zin.infolist()), flush=True)
        # Metadata equality plus CRC-checked streaming of every source entry above.
        with zipfile.ZipFile(src) as a, zipfile.ZipFile(part) as b:
            assert [(x.filename, x.CRC, x.file_size) for x in a.infolist()] == [
                (x.filename, x.CRC, x.file_size) for x in b.infolist()]
        part.rename(dest)
    print(rel, dest.stat().st_size, flush=True)
for rel in ('config/default.cfg', 'config/keys.txt', 'mods/public/mod.json'):
    dest = assets / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(data / rel, dest)
shutil.copytree(root.parent / '0ad-0.28.0-unix-build/0ad-0.28.0/binaries/data/l10n', assets / 'l10n',
                dirs_exist_ok=True, ignore=shutil.ignore_patterns('.tx'))
# Transifex authoring metadata is not game data. AAPT omits hidden directories,
# so it must not be listed as an installable asset either (including on reruns).
shutil.rmtree(assets / 'l10n/.tx', ignore_errors=True)
for file in sorted(assets.rglob('*')):
    if file.is_file():
        manifest.append({'path': file.relative_to(assets).as_posix(),
                         'size': file.stat().st_size,
                         'sha256': hashlib.file_digest(file.open('rb'), 'sha256').hexdigest()})
(work / 'assets/data-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Total packed game data:', sum(x['size'] for x in manifest), flush=True)
