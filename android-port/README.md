# 0 A.D. 0.28.0 — port native Android.4

Pembaruan ini memulihkan fullscreen, memilih unit dan
menampilkan menu build/HUD dengan satu tap, serta optimasi menuju **60 FPS**
untuk **Redmi Note 9 Pro / Adreno 618**. Gestur dua jari dan konfirmasi bangunan
dari pembaruan sebelumnya tetap tersedia.
Versi **0.28.0-android.4**, versionCode **280004**. Identitas
aplikasi dan kunci penandatanganan tetap sama, sehingga APK dapat dipasang
sebagai pembaruan tanpa menghapus data atau save game.

Dokumentasi ini menjelaskan versi yang telah dibangun pada PC sumber.
Berkas APK, laporan verifikasi, dan log yang disebut di bawah adalah hasil
build lokal; folder GitHub berisi source untuk membuatnya kembali.

Target: **ARM64 (`arm64-v8a`), Android 10+ / API 29**. Engine Pyrogenesis,
SpiderMonkey, SDL2, dan dependensi lain dikompilasi untuk Android dengan NDK.
Tidak memakai emulator, Wine, proot, Termux, maupun streaming dari PC.

Build ini bersifat **eksperimental**. Kompilasi dan isi APK diperiksa di PC;
gameplay, GPU, audio, kontrol sentuh, dan performa belum diuji di ponsel fisik.
Ini bukan rilis Android resmi dari Wildfire Games.

APK hasil build: `../0ad-0.28.0-android-arm64.apk` (**1,95 GB**).
Tanda tangan APK, alignment 16 KB, empat library ARM64 dan checksum seluruh
34 berkas data terkemas sudah lolos pemeriksaan. Laporan: `verification.json`;
checksum APK: `../0ad-0.28.0-android-arm64.apk.sha256`.

## Instalasi

1. Salin `../0ad-0.28.0-android-arm64.apk` ke ponsel, lalu instal. Izinkan
   pemasangan dari aplikasi pengelola berkas bila Android memintanya.
2. Buka **0 A.D.**. Launcher otomatis memeriksa dan menyiapkan data, lalu
   menampilkan **Siap bermain**. Tekan **MULAI GAME** untuk masuk ke game.
   Semua data sudah berada dalam APK; tidak perlu mengunduh data game.
3. Sisakan sekurangnya **2,1 GB setelah APK terpasang** untuk persiapan data.
   Disarankan 6 GB ruang kosong sebelum mulai menginstal karena Android juga
   memerlukan ruang untuk file APK, staging instalasi, data, dan cache.
4. Tunggu persiapan selesai. Setiap file disalin dan diverifikasi dengan SHA-256.
   Bila terputus, buka aplikasi kembali; file yang telah terverifikasi dipakai
   ulang. Jika gagal, bebaskan ruang penyimpanan lalu tekan **COBA LAGI**.
5. Mulai dari peta kecil dan sedikit pemain untuk pengujian pertama.

Arsip publik asli 3.508.827.314 byte dikompresi ulang menjadi 1.815.838.206 byte.
Nama file, ukuran asli, dan CRC semua entri dipertahankan. Total data terkemas
1.880.125.735 byte. Metadata authoring Transifex `.tx/config` tidak diperlukan
saat bermain dan dikecualikan. Arsip asli dalam workspace tidak diubah.

## Kontrol

- **Geser satu jari** di medan permainan: geser kamera.
- **Tap satu jari pada unit atau bangunan milik sendiri**: pilih dan tampilkan
  menu build/aksi saat jari dilepas, tanpa menahan atau menunggu tap kedua.
  Tap pada tanah memberi perintah gerak. Target kerja/serang tetap mengikuti
  aksi konteks, misalnya mengumpulkan sumber daya, membangun fondasi, memperbaiki,
  menyembuhkan, atau berdagang. Tanpa pilihan aktif, tap juga dapat memeriksa
  unit milik pemain lain.
- **Tap singkat dengan dua jari bersamaan**: lepas seluruh pilihan setelah
  kedua jari diangkat. Jari kedua masuk paling lambat 0,18 detik setelah jari
  pertama; seluruh tap selesai dalam 0,3 detik. Dalam mode bangun, ini juga
  membatalkan pratinjau bangunan atau tembok.
- **Tap dua kali dengan satu jari pada unit**: pilih semua unit milik pemain
  yang sejenis dan terlihat di layar, memakai aturan kelompok/rank bawaan
  0 A.D. Jarak antartap maksimal 0,3 detik, pada posisi yang berdekatan.
- **Tahan sekitar 0,42 detik lalu seret**: buat kotak seleksi; lepaskan untuk
  memilih unit di dalamnya. Tahan lalu lepas pada satu unit untuk memilih paksa
  bila tap biasa sedang memberikan perintah kerja atau aksi lain.
- **Putar dua jari melingkar**: putar arah kamera tanpa mengubah kemiringan.
- **Cubit dua jari**: zoom masuk/keluar; bisa digabung dengan rotasi.
- **Mode bangun**: tap atau geser untuk memindahkan pratinjau. **Tap dua kali**
  di lokasi yang berdekatan untuk menaruh bangunan. Satu tap, drag, atau tahan
  tidak mengirim perintah konstruksi. Tembok memerlukan konfirmasi tap dua kali
  untuk titik awal dan titik akhir. Lokasi tidak valid tetap dalam mode bangun.
- **Menu/HUD, minimap, dan slider**: memakai tap/drag biasa sesuai kontrol game.
- **Tombol kembali Android**: menu/batal. Kolom teks memakai keyboard Android;
  mouse dan keyboard fisik tetap diproses melalui SDL.

Bilah shortcut dan tombol arah Android telah dihapus. Panduan gestur tersedia
melalui **Panduan kontrol** pada launcher. Launcher memakai judul di tengah,
tombol mulai berwarna emas, progres persiapan data, dan area bawah dengan
**Panduan kontrol | Tentang 0 A.D.**. Konten dapat digulir pada layar pendek.

Seleksi satu tap langsung memperbarui pilihan dan data menu build/training
bawaan game. Tap kedua yang berdekatan dapat memperluasnya menjadi unit sejenis.
Tap yang menghasilkan perintah menunggu sekitar 0,3 detik untuk membedakannya
dari tap dua kali; tap yang sudah memilih tidak mengirim perintah tertunda.
Pratinjau bangunan bergerak langsung. Sisa jari setelah
zoom/rotasi diabaikan sampai semua jari dilepas. Rotasi, cubitan, pergeseran,
atau pergantian jari membatalkan calon tap dua jari. Hilangnya fokus, perubahan
halaman, dan pembatalan sentuhan membatalkan tap tertunda dan melepas input
yang tertahan. Sentuhan tiga jari membatalkan gestur yang sedang berjalan.

Android mengirim posisi kedua jari dari **MotionEvent yang sama** ke antrean
native. Pengenalan gestur, pemeriksaan GUI, dan input game berlangsung pada
thread engine. Kamera tetap memakai smoothing dan batas gerak/zoom bawaan;
edge scrolling mouse tidak berjalan selama kontrol sentuh digunakan.

## Performa

Surface Android, output layar, HUD, koordinat kamera untuk picking, dan input
memakai **resolusi layar penuh**. Surface mengikuti ukuran layout dengan
`MATCH_PARENT`, dan mode immersive diterapkan kembali saat fokus/resume.
Ukuran buffer Surface tidak lagi diperkecil, sehingga View tidak menyusut.

Hanya **adegan 3D** yang memakai target render dengan sisi pendek **720 piksel**.
Pada layar 2400×1080 Redmi Note 9 Pro, adegan dirender ke **1600×720** lalu
dibentangkan ke seluruh layar; HUD digambar setelahnya pada ukuran penuh.
Ini mengurangi jumlah piksel adegan sekitar **56%**, dengan gambar 3D yang lebih
lembut. Target warna/depth dipakai ulang; perangkat yang tidak mendukungnya
kembali ke render resolusi penuh. Opsi `android.scene.shortedge = 0` menonaktifkan
penurunan resolusi adegan. Mengaktifkan postprocessing atau silhouettes juga
menggunakan jalur render biasa karena fitur tersebut membutuhkan depth/stencil
dan pass tambahan.

Target awal **60 FPS** berlaku untuk game dan menu. Pada layar 60 Hz, VSync
mengatur ritme frame tanpa penundaan timer kedua yang dapat melewatkan vblank.
Limiter dengan waktu monoton tetap dipakai jika VSync dimatikan, refresh rate
tidak diketahui, atau batas FPS lebih rendah dari refresh rate. Profil memakai
tekstur medium (`textures.quality = 1`), anisotropi 1, material lebih ringan,
kualitas actor medium, variasi model terbatas, dan LOS tanpa smoothing.
Silhouettes, bayangan, partikel, efek air berat, dan postprocessing dimatikan.

Profil diterapkan sekali saat launcher menyiapkan data, termasuk pada instalasi
lama, termasuk android.3. Konfigurasi sebelumnya disimpan sebagai
`config/user.cfg.pre-android.4`; penanda migrasi baru adalah `.android-performance-4`.
Pengaturan suara, nama pemain dan hotkey dipertahankan; perubahan opsi grafis
setelah migrasi tidak ditimpa pada peluncuran berikutnya. Save game tidak diubah.

**60 FPS adalah target, bukan hasil pengukuran di ponsel.** Uji scheduler,
konfigurasi, dan target render dilakukan di host. Stabilitas aktual bergantung pada jumlah
unit, ukuran peta, AI, suhu perangkat, dan driver GPU.

## Cakupan build

Engine, simulasi, AI dan skrip game, UI, renderer GLES, audio OpenAL/Vorbis,
font FreeType/ICU, dan jaringan ENet disertakan. Mode single player serta
jaringan LAN/direct IP terkompilasi, tetapi belum diuji saat berjalan.

Lobby daring XMPP, UPnP otomatis, editor desktop Atlas, dan konversi aset
COLLADA/NVTT saat berjalan tidak disertakan. Data rilis yang diberikan sudah
berisi model/tekstur hasil konversi. Mod tambahan yang membutuhkan konversi
aset desktop tidak didukung oleh build ini. Bayangan, efek air berat, efek
pascaproses, dan partikel dimatikan pada konfigurasi awal.

Data berada dalam penyimpanan khusus aplikasi:
`Android/data/org.wildfiregames.zeroad/files/0ad/`. Menghapus aplikasi juga
menghapus data, konfigurasi, dan save game miliknya.

Game berjalan di proses Android `:game` tersendiri. Saat aktivitas game ditutup,
prosesnya dihentikan agar sesi berikutnya memulai ulang singleton engine dengan
bersih; launcher tetap berada di proses utama.

## Build di PC baru

Gunakan panduan [BUILD-PC-BARU.md](../BUILD-PC-BARU.md) dari root paket.
Toolchain Linux x86_64 dan skrip portable disiapkan di sana. Dari root paket:

```sh
bash build-pc-baru.sh --check
bash build-pc-baru.sh
```

Source engine 0.28.0 dan data dapat dipulihkan oleh
`../scripts/prepare-sources.py`. Perubahan engine tersedia dalam
`patches/engine-android.patch` dan diterapkan otomatis hanya jika belum ada.
Patch SpiderMonkey dan SDL diterapkan oleh tahap build masing-masing.

Source gestur: `native/TouchGestures.h`, `native/AndroidTouchInput.cpp`.
Limiter: `native/FramePacer.h`. Profil grafis:
`app/src/main/java/org/wildfiregames/zeroad/PerformanceProfile.java`.
Target resolusi adegan: `native/AndroidSceneBuffer.cpp`,
`native/SceneResolution.h`, dan patch `source/renderer/Renderer.cpp`.

Keystore pribadi dan hasil APK tidak tersedia dalam folder GitHub.
Petunjuk pemakaian kunci backup untuk memperbarui APK lama juga ada dalam
panduan PC baru.

## Verifikasi

`scripts/verify-apk.py` memeriksa ARM64 ELF, library yang dibutuhkan, ekspor
entry point engine/JNI termasuk pengiriman/batal gestur, alignment segmen
16 KB, versi APK, kesesuaian engine dengan hasil build, dan checksum seluruh
data yang tertanam di APK. Hasilnya ditulis ke `verification.json`.
`apksigner` dan `zipalign` memeriksa tanda tangan serta alignment APK.
Android lint dijalankan terpisah; laporan ada di `.build/gradle/app/reports/`.

Uji gestur pada host Linux, tanpa emulator:

```sh
bash scripts/test-gestures.sh
bash scripts/test-session-input.sh
bash scripts/test-performance.sh
bash scripts/test-rendering.sh
```

**84 skenario dan 10.000 event campuran** lulus dengan
AddressSanitizer/UndefinedBehaviorSanitizer (`gesture-tests.log`). Uji mencakup
seleksi saat satu tap dilepas tanpa perintah susulan, tap dua jari,
seleksi unit sejenis, konfirmasi bangunan, kamera, long-press,
GUI, pembatalan, pergantian jari,
rotasi dua putaran penuh, rotasi bersama zoom, dan beberapa kepadatan layar.
**39 pemeriksaan protokol input** juga lulus terhadap `gui/session/input.js`
dan `selection.js` yang benar-benar dikemas (`session-input-tests.log`),
termasuk perubahan pilihan yang memicu pembaruan HUD dan mengosongkan cache
menu build/training, aksi konteks, seleksi kotak, kelompok unit lintas rank,
deselection, pratinjau/konstruksi, lokasi tidak valid, serta konfirmasi tembok.
Layanan picking/perintah memakai data tiruan; ini bukan uji gameplay penuh.
**12 pemeriksaan scheduler** dan **9 pemeriksaan profil performa** lulus
(`performance-tests.log`), termasuk VSync 60 Hz, interval pecahan, frame lambat,
resume, ukuran HUD penuh, migrasi konfigurasi, dan cadangan pengaturan.
**14 pemeriksaan buffer adegan** lulus (`rendering-tests.log`), mencakup ukuran
adegan terpisah dari layar, cakupan seluruh layar, orientasi gambar, opacity,
pemakaian ulang target, resize, dan jalur fallback. Source buffer produksi
dikompilasi dengan layanan engine tiruan; pengujian ini tidak menjalankan GPU.
Prasyarat tambahan uji: compiler C++ host dengan sanitizer, Node.js, dan JDK 17+.

Tidak ada emulator yang digunakan. Saat pemeriksaan terakhir tidak ada ponsel
yang terhubung melalui ADB. APK baru ini
belum dipasang atau diuji gameplay di ponsel. Untuk menguji pada ponsel yang
tersambung:

```sh
adb install -r ../0ad-0.28.0-android-arm64.apk
adb shell am start -n org.wildfiregames.zeroad/.LauncherActivity
adb logcat -v threadtime -s pyrogenesis SDL AndroidRuntime
```

Uji menu utama, satu pertandingan peta kecil, pemilihan/perintah unit,
save/load, audio, background/resume, keluar game, dan membuka sesi kedua.
Log engine berada di subfolder `0ad/logs` penyimpanan aplikasi, termasuk
`mainlog.html`, `interestinglog.html`, dan `native.log`.

## Lisensi

Hak cipta game dan engine milik Wildfire Games dan para kontributor. Source
engine GPL-2.0-or-later; artwork/audio mengikuti lisensi dan atribusi di arsip
aslinya. Lisensi dependensi juga disertakan di `assets/licenses` dalam APK.
Source asli, patch, skrip build, serta referensi source dependensi disertakan
dalam workspace ini untuk memeriksa dan membangun ulang port.
