# Membangun 0 A.D. Android di PC baru

Paket ini menyimpan versi **0.28.0-android.4 / versionCode 280004**.
Source gameplay, launcher, adapter sentuh, dan patch engine sama dengan
workspace terbaru yang digunakan untuk APK tanggal 11 November 2026.
Skrip di root paket ditambahkan untuk mengatur lokasi toolchain pada PC baru;
skrip dan kode asli di `android-port/` tetap tersedia.

## 1. Siapkan sistem dan folder

Gunakan Linux **x86_64**, misalnya Ubuntu 24.04/26.04. Lingkungan sebelumnya
adalah Ubuntu 26.04 dengan JDK 21. Windows dapat menggunakan Ubuntu di WSL2;
jalankan seluruh langkah berikut di terminal WSL, simpan proyek di filesystem
Linux, dan gunakan SDK/NDK **Linux**. Build langsung dari PowerShell atau
macOS belum didukung oleh skrip ini.

Disarankan RAM 16 GB dan ruang kosong setidaknya 30 GB untuk data, toolchain,
source dependensi, file sementara, dan APK. `JOBS=3` adalah nilai awal;
turunkan ke `JOBS=2` bila memori terbatas.

- Dari ZIP backup: ekstrak seluruh isinya, kemudian masuk ke folder hasil
  ekstrak yang berisi `build-pc-baru.sh`. Jalankan `sha256sum -c SHA256SUMS`
  untuk memeriksa semua berkas backup.
- Dari GitHub: clone repositori, kemudian masuk ke root clone tersebut.
  Source engine dan data game akan diambil saat menjalankan build.

Pasang paket sistem:

```sh
sudo apt update
sudo apt install build-essential clang autoconf automake libtool pkg-config \
    ninja-build python3 curl ca-certificates xz-utils unzip zip patch git \
    openjdk-21-jdk libssl-dev zlib1g-dev libffi-dev nodejs rustup
```

Python harus >= 3.12. Paket `rustup` tersedia melalui repositori Ubuntu
Universe. Bila distro Anda tidak menyediakannya, pasang rustup dari
<https://rustup.rs/> sebelum melanjutkan. Jangan menjalankan build sebagai root.

## 2. Pasang Android SDK dan NDK

Gunakan Android Studio SDK Manager atau Android command-line tools dari
<https://developer.android.com/studio#command-tools>. Pilih komponen Linux:

| Komponen | Versi |
| --- | --- |
| Android SDK Platform | 36 |
| Android SDK Build-Tools | 36.0.0 |
| Android NDK | 27.2.12479018 (r27c) |
| Android Platform-Tools | versi yang tersedia |
| Android SDK Command-line Tools | versi yang tersedia |

Lokasi SDK bawaan skrip adalah `$HOME/Android/Sdk`. Jika berbeda, atur
`ANDROID_SDK_ROOT` sebelum menjalankan skrip. Contoh memakai sdkmanager
setelah command-line tools sudah terpasang:

```sh
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
"$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" \
    --sdk_root="$ANDROID_SDK_ROOT" --licenses
"$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" \
    --sdk_root="$ANDROID_SDK_ROOT" \
    "platform-tools" "platforms;android-36" "build-tools;36.0.0" \
    "ndk;27.2.12479018"
```

Target APK tetap minSdk 29 / targetSdk 35 / `arm64-v8a`; platform 36 dipakai
untuk kompilasi sesuai konfigurasi port terbaru.

## 3. Pasang host tools dengan versi yang sesuai

Dari root proyek, jalankan:

```sh
bash scripts/setup-host-tools.sh
```

Skrip memasang CMake 3.31.6, Gradle 8.9, Rust 1.84.1 beserta target
`aarch64-linux-android`, dan cbindgen 0.29.2. CMake/Gradle diverifikasi dengan
SHA-256. cbindgen dikompilasi pada PC baru agar sesuai dengan library sistem.
Skrip ini membutuhkan internet; backup menyediakan cache unduhan CMake dan
15 arsip source dependensi native, tetapi SDK/NDK, Rust, Gradle, serta
dependensi Gradle/Cargo tetap perlu dipasang atau diunduh.

Semua hasil build dan host tools default berada di
`$HOME/0ad-android-build/android.4`. Jika ingin memakai disk lain:

```sh
export BUILD_ROOT="/lokasi-disk/0ad-android-build"
bash scripts/setup-host-tools.sh
```

Gunakan direktori yang dapat ditulis pengguna, absolut, dan **tanpa spasi**.
Pertahankan nilai `BUILD_ROOT` yang sama saat setup, pemeriksaan, dan build.
Jangan memakai cache kompilasi dari PC lama: cache CMake/SpiderMonkey menyimpan
path dan toolchain lama. ZIP memang mengecualikan cache tersebut.

Variabel opsional: `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`, `BUILD_ROOT`,
`JAVA_HOME`, `CMAKE`, `GRADLE`, dan `JOBS`. Jika memiliki beberapa JDK,
arahkan `JAVA_HOME` ke JDK 21, misalnya `/usr/lib/jvm/java-21-openjdk-amd64`.

## 4. Periksa lalu build

```sh
bash build-pc-baru.sh --check
bash build-pc-baru.sh
```

Pemeriksaan pertama tidak mengompilasi game. Pemeriksaan mencakup versi
toolchain, compiler ARM64, library Rust untuk Android, dan komponen SDK.

Build lengkap akan:

1. Memakai source/data yang sudah ada di backup, atau mengunduh release resmi
   sesuai `upstream.lock.json` jika dijalankan dari clone GitHub.
2. Memastikan patch engine sudah terpasang atau menerapkannya pada source
   bersih. Source yang tidak cocok tidak ditimpa.
3. Memakai cache 15 source dependensi bila tersedia, memverifikasi checksum,
   dan mengompilasi dependensi, SpiderMonkey, dan engine.
4. Mengompresi data game, membangun APK, menandatangani, dan memverifikasi APK.

Output berada di root paket:

```text
0ad-0.28.0-android-arm64.apk
0ad-0.28.0-android-arm64.apk.sha256
android-port/verification.json
```

Untuk menyiapkan source saja tanpa mengunduh data game:

```sh
python3 scripts/prepare-sources.py --engine-only
```

Data game masih diperlukan untuk membuat APK lengkap. Semua folder source
upstream, data, unduhan besar, keystore, dan hasil build diabaikan Git.
Jika menjalankan tahap build asli secara manual, ekspor lingkungan dahulu:

```sh
source scripts/pc-env.sh
bash android-port/scripts/build-all.sh
```

## 5. Keystore dan pembaruan aplikasi

ZIP backup pribadi menyimpan
`android-port/keys/local-development.jks`, sama dengan kunci APK terakhir.
Pertahankan file tersebut agar APK hasil build dapat memperbarui instalasi
lama tanpa uninstall. Ini keystore pengembangan dengan alias `zeroad-local`
dan password pengembangan `android`, sesuai skrip build sebelumnya.

Folder GitHub tidak memuat kunci. Build dari clone GitHub akan membuat kunci
pengembangan baru bila file tersebut tidak ada. Jika Anda pemilik backup
dan ingin memakai kunci lama, salin file di atas dari backup ke lokasi yang
sama dalam clone. Folder `keys/` sudah masuk `.gitignore`.

## 6. Pemeriksaan tambahan

Setelah source/data disiapkan dan dikemas, pengujian host tersedia:

```sh
source scripts/pc-env.sh
bash android-port/scripts/test-gestures.sh
bash android-port/scripts/test-session-input.sh
bash android-port/scripts/test-performance.sh
bash android-port/scripts/test-rendering.sh
```

Pengujian `test-session-input.sh` membaca skrip game dari hasil `pack-data.py`.
Keberhasilan uji host dan pemeriksaan APK tidak membuktikan performa 60 FPS
atau gameplay pada ponsel. Build penuh pada PC baru tetap perlu dijalankan;
paket backup ini diperiksa dari sisi integritas file, kelengkapan patch,
portabilitas konfigurasi, dan prasyarat toolchain pada PC sumber.
