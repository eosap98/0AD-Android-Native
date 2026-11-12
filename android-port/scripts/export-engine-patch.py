#!/usr/bin/env python3
"""Record the engine changes against the untouched source archive supplied by the user."""
from pathlib import Path
import difflib
import tarfile

port = Path(__file__).resolve().parents[1]
workspace = port.parent
source = workspace / '0ad-0.28.0-unix-build/0ad-0.28.0'
changed = {
    'source/main.cpp', 'source/renderer/Renderer.cpp',
    'source/ps/GameSetup/Paths.cpp', 'source/ps/GameSetup/HWDetect.cpp',
    'source/ps/TouchInput.cpp', 'source/ps/ModIo.cpp', 'source/ps/UserReport.cpp',
    'source/lib/sysdep/os/unix/ufilesystem.cpp',
    'source/renderer/backend/gl/Texture.cpp',
    'source/renderer/backend/gl/DeviceCommandContext.cpp',
    'source/ps/GameSetup/GameSetup.cpp',
    'source/graphics/CameraController.cpp', 'source/gui/CGUI.cpp', 'source/gui/CGUI.h',
}
parts = []
with tarfile.open(workspace / '0ad-0.28.0-unix-build.tar.xz', 'r|xz') as archive:
    for member in archive:
        name = member.name.removeprefix('0ad-0.28.0/')
        if name in changed:
            original = archive.extractfile(member).read().decode().splitlines(keepends=True)
            modified = (source / name).read_text().splitlines(keepends=True)
            parts.extend(difflib.unified_diff(original, modified, 'a/' + name, 'b/' + name))
            changed.remove(name)
assert not changed, changed
(port / 'patches/engine-android.patch').write_text(''.join(parts))
