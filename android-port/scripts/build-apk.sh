#!/usr/bin/env bash
source "$(dirname -- "$0")/env.sh"
GRADLE="${GRADLE:-$BUILD_ROOT/host-tools/gradle-8.9/bin/gradle}"
JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}"
export JAVA_HOME

python3 "$PORT_ROOT/scripts/patch-sdl-java.py"
python3 "$PORT_ROOT/scripts/prepare-licenses.py"
test -f "$BUILD_ROOT/jniLibs/arm64-v8a/libpyrogenesis.so"
test -f "$BUILD_ROOT/assets/data-manifest.json"

# Isolate stable SDK 36 from installed preview SDKs which older lint cannot parse.
mkdir -p "$BUILD_ROOT/sdk/platforms" "$BUILD_ROOT/sdk/build-tools"
ln -sfn "$ANDROID_SDK_ROOT/platforms/android-36" "$BUILD_ROOT/sdk/platforms/android-36"
ln -sfn "$ANDROID_SDK_ROOT/build-tools/36.0.0" "$BUILD_ROOT/sdk/build-tools/36.0.0"
ln -sfn "$ANDROID_SDK_ROOT/platform-tools" "$BUILD_ROOT/sdk/platform-tools"
ln -sfn "$ANDROID_SDK_ROOT/licenses" "$BUILD_ROOT/sdk/licenses"
(
    cd "$PORT_ROOT"
    ANDROID_HOME="$BUILD_ROOT/sdk" ANDROID_SDK_ROOT="$BUILD_ROOT/sdk" \
        "$GRADLE" :app:assembleRelease
)
BUILD_TOOLS="$ANDROID_SDK_ROOT/build-tools/36.0.0"
UNSIGNED="$BUILD_ROOT/gradle/app/outputs/apk/release/app-release-unsigned.apk"
ALIGNED="$BUILD_ROOT/0ad-aligned.apk"
OUTPUT="$PORT_ROOT/../0ad-0.28.0-android-arm64.apk"
"$BUILD_TOOLS/zipalign" -P 16 -f 4 "$UNSIGNED" "$ALIGNED"

mkdir -p "$PORT_ROOT/keys"
KEYSTORE="$PORT_ROOT/keys/local-development.jks"
if [[ ! -f "$KEYSTORE" ]]; then
    "$JAVA_HOME/bin/keytool" -genkeypair -noprompt -keystore "$KEYSTORE" \
        -storepass android -keypass android -alias zeroad-local -keyalg RSA \
        -keysize 2048 -validity 10000 -dname 'CN=0 A.D. local Android build'
    chmod 600 "$KEYSTORE"
fi
"$BUILD_TOOLS/apksigner" sign --ks "$KEYSTORE" --ks-key-alias zeroad-local \
    --ks-pass pass:android --key-pass pass:android --v1-signing-enabled false \
    --out "$OUTPUT" "$ALIGNED"
"$BUILD_TOOLS/apksigner" verify --verbose --print-certs "$OUTPUT"
"$BUILD_TOOLS/zipalign" -c -P 16 4 "$OUTPUT"
(
    cd "$PORT_ROOT/.."
    sha256sum "$(basename "$OUTPUT")" > "$(basename "$OUTPUT").sha256"
)
python3 "$PORT_ROOT/scripts/verify-apk.py" "$OUTPUT"
