# 0 A.D. Android — 0.28.0-android.4

Native 0AD Games **ARM64, Android 10+**, berdasarkan 0 A.D. 0.28.0.
Snapshot kode terbaru: **12 November 2026**, versionCode **280004**.

Rincian perilaku dan batasan ada di [dokumentasi port](android-port/README.md).

## Isi repositori

- `android-port/`: launcher Java, adapter native C++, pengujian, skrip build,
  daftar dependensi dengan SHA-256, dan patch engine/SpiderMonkey.
- `scripts/` dan `build-pc-baru.sh`: persiapan source dan lingkungan PC baru.
- `upstream.lock.json`: URL release resmi beserta checksum source/data 0.28.0.
- `licenses/`: ketentuan lisensi upstream.

**Folder ini tidak menyertakan data game, APK, library hasil kompilasi,
SDK/NDK, cache build, atau keystore.** Source engine upstream juga diambil
terpisah; seluruh perubahan port terhadap source tersebut tersedia sebagai
`android-port/patches/engine-android.patch`.

## Build

Gunakan **Linux x86_64**. Untuk Windows, gunakan Ubuntu melalui **WSL2** dan
pasang toolchain Linux di dalam WSL. Ikuti [BUILD-PC-BARU.md](BUILD-PC-BARU.md)
untuk memasang JDK, Android SDK/NDK, Rust, CMake, Gradle, dan cbindgen.
Setelah prasyarat terpasang, dari root repositori:

```sh
bash build-pc-baru.sh --check
bash build-pc-baru.sh
```

Skrip kedua mengunduh source/data resmi yang belum tersedia, memverifikasi
SHA-256, menerapkan patch sekali, kemudian membangun APK. Unduhan source
sekitar 165 MB dan data sekitar 1,42 GB. File tersebut masuk `.gitignore`.
Build awal memerlukan internet untuk toolchain dan dependensi Gradle/Rust.

Output: `0ad-0.28.0-android-arm64.apk` dan file `.sha256` di root repositori.
Build tanpa keystore pribadi membuat kunci pengembangan baru. Untuk
memperbarui instalasi APK lama, gunakan keystore dari backup pribadi sesuai
petunjuk dalam panduan build.

## Unggah ke GitHub

Unggah isi **folder ini**. Buat repositori kosong di GitHub, lalu jalankan
perintah berikut dari folder ini dan ganti URL contoh:

```sh
git init -b main
git add .
git commit -m "Import 0ad Android 0.28.0-android.4 source"
git remote add origin https://github.com/USERNAME/NAMA-REPO.git
git push -u origin main
```

Gunakan folder GitHub ini sebagai sumber unggahan; ZIP backup pribadi
menyertakan data game dan kunci penandatanganan. File `.gitignore` juga
mengecualikan keduanya jika kelak dipakai untuk build lokal.

## Lisensi

0 A.D. / Pyrogenesis adalah karya Wildfire Games dan para kontributor.
Port ini merupakan proyek komunitas, bukan rilis Android resmi.
Source engine dan modifikasinya mengikuti GPL-2.0-or-later; bagian engine
lain dapat mengikuti lisensi yang dijelaskan di
[licenses/LICENSE.md](licenses/LICENSE.md). Lisensi dependensi dicantumkan
dalam sumber masing-masing dan dikemas oleh skrip build ke APK.
Data game yang diunduh tetap membawa lisensi dan atribusi aslinya.
