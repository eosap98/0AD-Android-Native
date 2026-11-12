#!/usr/bin/env python3
"""Small Android API 33+ compatibility fixes for the pinned SDL Java glue."""
from pathlib import Path
import os

base = Path(os.environ.get('BUILD_ROOT', '/var/tmp/0ad-android-build')) / 'src/sdl/android-project/app/src/main/java/org/libsdl/app'
p = base / 'HIDDeviceManager.java'
text = p.read_text()
old = '        mContext.registerReceiver(mUsbBroadcast, filter);'
new = '''        if (Build.VERSION.SDK_INT >= 33) {
            mContext.registerReceiver(mUsbBroadcast, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            mContext.registerReceiver(mUsbBroadcast, filter, 0);
        }'''
if 'Context.RECEIVER_NOT_EXPORTED' not in text:
    assert old in text
    p.write_text(text.replace(old, new))
elif old in text:
    # The three-argument overload is available since API 26; Android <33 does
    # not use the exported-state flags. This also makes the lint check precise.
    p.write_text(text.replace(old, '            mContext.registerReceiver(mUsbBroadcast, filter, 0);'))
text = p.read_text()
if '@android.annotation.SuppressLint("UnspecifiedRegisterReceiverFlag")' not in text:
    # Lint 8.7 also reports the pre-33 fallback despite the API guard above.
    p.write_text(text.replace('    private void initializeUSB() {',
        '    @android.annotation.SuppressLint("UnspecifiedRegisterReceiverFlag")\n    private void initializeUSB() {'))

p = base / 'SDLAudioManager.java'
text = p.read_text()
old = '''        if (isCapture) {
            if (mAudioRecord == null) {'''
new = '''        if (isCapture) {
            // This game uses playback only; reject capture without permission.
            if (mContext.checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)
                    != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                return null;
            }
            if (mAudioRecord == null) {'''
if 'This game uses playback only' not in text:
    assert old in text
    p.write_text(text.replace(old, new))
