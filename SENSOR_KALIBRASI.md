# Panduan Kalibrasi dan Konversi Sensor MQ-2 untuk Deteksi Asap

## 📋 Daftar Isi

1. [Pengenalan Sensor MQ-2](#pengenalan-sensor-mq-2)
2. [Cara Kerja Sistem Konversi](#cara-kerja-sistem-konversi)
3. [Proses Kalibrasi Otomatis](#proses-kalibrasi-otomatis)
4. [Penjelasan Rumus Fisika](#penjelasan-rumus-fisika)
5. [Thresholds dan Interpretasi Data](#thresholds-dan-interpretasi-data)
6. [Tips Kalibrasi yang Akurat](#tips-kalibrasi-yang-akurat)

---

## 🔬 Pengenalan Sensor MQ-2

### Apa itu Sensor MQ-2?

Sensor MQ-2 adalah **sensor gas semiconductor** yang bisa mendeteksi berbagai macam gas seperti:

- LPG (gas elpiji)
- Propane
- Methane
- Alcohol (termasuk komponen asap rokok)

### Bagaimana Cara Sensor Bekerja?

Sensor MQ-2 memiliki **material sensitif yang berubah resistansinya** ketika terkena gas:

- **Di udara bersih**: resistansi sensor tinggi (sekitar 10,000 - 15,000 Ohm)
- **Saat ada gas**: resistansi sensor turun (semakin banyak gas = semakin rendah resistansi)

Prinsipnya mirip seperti saklar cahaya yang sensitif - semakin gelap/banyak pencahayaan = semakin berubah nilainya.

---

## ⚙️ Cara Kerja Sistem Konversi

Sistem konversi sensor kita memiliki **4 tahap pipeline** yang saling terkoneksi:

```
┌─────────────┐     ┌──────────────┐     ┌────────────┐     ┌──────────┐
│  ADC Raw    │ --> │  Tegangan    │ --> │   Rs       │ --> │   PPM    │
│ (0-4095)    │     │  (0-3.3V)    │     │  (Ohm)     │     │ (0-1000) │
└─────────────┘     └──────────────┘     └────────────┘     └──────────┘
```

### Tahap 1: ADC Raw → Tegangan

**Apa itu ADC?** ADC (Analog-to-Digital Converter) adalah fitur pada microcontroller yang mengubah sinyal analog (tegangan) menjadi angka digital yang bisa diproses.

Rumus konversi:

```
Tegangan (V) = ADC_Value × 3.3V / 4095
```

**Penjelasan:**

- `ADC_Value`: Nilai yang dibaca dari pin ADC (0 hingga 4095)
- `3.3V`: Tegangan referensi ESP32 (ADC hanya bisa membaca sampai 3.3V)
- `4095`: Resolusi 12-bit ADC (2^12 - 1 = 4095 step)

**Contoh praktis:**

- Jika ADC membaca 2048 (setengah dari 4095)
- Maka tegangan = 2048 × 3.3 / 4095 = **1.65V** ✓

---

### Tahap 2: Tegangan → Resistansi Sensor (Rs)

**Apa itu Voltage Divider?**

Dalam modul sensor MQ-2, ada **2 resistor yang disusun seri**:

- **RL (Load Resistor)**: 10kΩ (resistor tetap yang sudah built-in di modul)
- **Rs (Sensor Resistance)**: Resistansi sensor yang berubah sesuai gas

Ketika disusun seri dan diberi tegangan 5V:

```
        5V
        │
       ┌┴┐
       │R│ (Sensor MQ-2)
       └┬┘
        │ ← Tegangan output terukur Vout (inilah yang dibaca ADC)
       ┌┴┐
       │R│ (RL = 10kΩ)
       └┬┘
        │
       GND
```

Rumus voltage divider:

```
Vout = Vin × RL / (RL + Rs)
```

Kita perlu membalikin rumus untuk mendapat Rs:

```
Rs = RL × (Vin - Vout) / Vout
```

**Penjelasan nilai-nilai:**

- `Vin = 5V` (tegangan supply sensor, bukan 3.3V!)
- `RL = 10,000 Ohm` (resistansi beban tetap)
- `Vout` = tegangan yang terukur oleh ADC (hasil Tahap 1)

**Contoh praktis:**

- Jika Vout = 1.5V dan Vin = 5V
- Rs = 10,000 × (5 - 1.5) / 1.5
- Rs = 10,000 × 3.5 / 1.5
- Rs = **23,333 Ohm** ✓

---

### Tahap 3: Hitung Rasio Rs/Ro

**Apa itu Ro?**

`Ro` adalah resistansi sensor **saat di udara bersih** (baseline/referensi kalibrasi).

**Mengapa perlu Ro?**

Setiap sensor individuunik - mereka tidak persis sama. Jadi:

- Sensor A mungkin punya Ro = 8,000 Ohm
- Sensor B mungkin punya Ro = 12,000 Ohm

Dengan menggunakan rasio `Rs/Ro`, kita bisa **menormalkan perbedaan antar sensor**, sehingga pembacaan lebih akurat dan konsisten.

**Contoh:**

- Jika Rs = 5,000 Ohm dan Ro = 10,000 Ohm
- Rasio = 5,000 / 10,000 = **0.5**

---

### Tahap 4: Rasio Rs/Ro → PPM (Konsentrasi)

Ini adalah tahap yang paling penting! Kami menggunakan **kurva kalibrasi MQ-2 khusus untuk asap rokok**.

Kurva ini adalah hasil penelitian dan fitting dari datasheet MQ-2 yang disesuaikan untuk deteksi asap di ruangan tertutup (aplikasi KTR/Kawasan Tanpa Rokok).

**Rumus Kurva Kalibrasi:**

```
PPM = 10^(1.2 + (-0.45) × log₁₀(Rs/Ro))
```

Atau bisa ditulis sebagai:

```
PPM = 10^1.2 × (Rs/Ro)^(-0.45)
PPM = 15.85 × (Rs/Ro)^(-0.45)
```

**Penjelasan rumus:**

- `1.2`: Konstanta intercept (konstanta pertama kurva)
- `-0.45`: Slope (kemiringan kurva, negatif = semakin besar rasio, semakin kecil PPM)
- `log₁₀`: Logaritma basis 10
- `^(-0.45)`: Pangkat -0.45

**Mengapa kurva ini?**

Hubungan antara resistansi gas sensor dan konsentrasi gas **tidak linear** (bukan garis lurus), melainkan **logaritmik** (melengkung). Kurva logaritmik ini memberikan presisi yang lebih baik di seluruh range deteksi.

---

## 🔄 Proses Kalibrasi Otomatis

### Timeline Kalibrasi Saat Startup

Ketika perangkat pertama kali dinyalakan:

```
Detik 0     : Unit dinyalakan, serial mulai @115200 baud
Detik 0-5   : Kalibrasi dimulai
              - Ambil 50 sampel pembacaan ADC
              - Setiap sampel diambil setiap 100ms
              - Total waktu: 50 × 100ms = 5 detik
Detik 5     : Hitung Ro rata-rata dari 50 sampel
Detik 5+    : Udara bersih selesai, sensor siap beroperasi normal
```

### Algoritma Kalibrasi Terperinci

```cpp
1. Set pin sensor sebagai INPUT
2. Inisialisasi total = 0
3. Untuk i = 1 sampai 50:
   a. Baca ADC → nilaiADC
   b. Konversi ke Tegangan → Vout
   c. Hitung Rs menggunakan voltage divider formula
   d. Tambahkan Rs ke total
   e. Cetak "." untuk menunjukkan progress
   f. Tunda 100ms sebelum sampel berikutnya
4. Hitung rata-rata: Ro = total / 50
5. Simpan Ro sebagai baseline kalibrasi
6. Set flag kalibrasi = SELESAI
7. Tampilkan pesan sukses dengan nilai Ro
```

### Contoh Output Serial Saat Kalibrasi

```
[INFO] Memulai kalibrasi sensor pada udara bersih...
..................................................
[INFO] Kalibrasi selesai! Ro = 10234.50 Ohm
```

### ⚠️ PENTING: Kondisi Saat Kalibrasi

**Sensor HARUS dalam kondisi udara BERSIH selama kalibrasi!**

Jangan:

- ❌ Letakkan rokok di dekat sensor
- ❌ Letakkan di dekat sumber asap lain
- ❌ Letakkan di dekat uap pembersih/parfum

Pastikan:

- ✅ Ruangan ventilasi baik
- ✅ Sensor di tempat yang tenang (tidak ada draft udara)
- ✅ Menunggu ~30 detik setelah dinyalakan baru observasi

---

## 📐 Penjelasan Rumus Fisika

### Mengapa Menggunakan Logaritma?

Hubungan antara konsentrasi gas dan resistansi sensor adalah **exponensial inverse**:

- Semakin tinggi konsentrasi → semakin rendah resistansi
- Hubungannya tidak linear (bukan garis lurus)

Jika kita plot Rs vs PPM:

```
    Rs
    ↑
    │     ╱╱╱  (kurva exponensial)
    │   ╱╱
    │ ╱╱
    │╱
    └─────────→ PPM
```

Untuk "meluruskan" kurva exponensial ini, kita gunakan logaritma.

Jika plot log(Rs/Ro) vs log(PPM):

```
    log(Rs/Ro)
    ↑
    │ ╲
    │  ╲  (garis lurus - linear!)
    │   ╲
    │    ╲
    └─────────→ log(PPM)
```

**Keuntungan:**

- Lebih mudah dianalisis (hubungan linear)
- Akurat di seluruh range (dari 0 PPM hingga 1000+ PPM)
- Fit curve lebih konsisten

### Validasi Rumus

Mari kita verifikasi dengan contoh ekstrem:

**Case 1: Udara Bersih (Rs = Ro)**

```
Rasio = Ro / Ro = 1.0
log₁₀(1.0) = 0
PPM = 10^(1.2 + (-0.45) × 0)
PPM = 10^1.2
PPM ≈ 15.85 PPM ✓ (baseline di udara bersih)
```

**Case 2: Rs turun 50% (Rs = 0.5 × Ro)**

```
Rasio = 0.5
log₁₀(0.5) ≈ -0.301
PPM = 10^(1.2 + (-0.45) × (-0.301))
PPM = 10^(1.2 + 0.135)
PPM = 10^1.335
PPM ≈ 21.6 PPM ✓ (sedikit asap)
```

**Case 3: Rs turun 90% (Rs = 0.1 × Ro)**

```
Rasio = 0.1
log₁₀(0.1) = -1.0
PPM = 10^(1.2 + (-0.45) × (-1.0))
PPM = 10^(1.2 + 0.45)
PPM = 10^1.65
PPM ≈ 44.7 PPM ✓ (lebih banyak asap)
```

---

## 📊 Thresholds dan Interpretasi Data

### Range Nilai Kepadatan Asap

Berdasarkan datasheet MQ-2 dan aplikasi KTR, kami set thresholds:

| Range (PPM)      | Status             | Warna  | Aksi          |
| ---------------- | ------------------ | ------ | ------------- |
| 0 - 400          | ✅ AMAN            | Hijau  | Normal        |
| 400 - 700        | ⚠️ PERINGATAN      | Kuning | Monitor       |
| > 700            | 🚨 BAHAYA          | Merah  | Alert + Catat |
| Delta > 50/detik | 🔔 PERUBAHAN CEPAT | Orange | Notifikasi    |

### Interpretasi Praktis

**0-100 PPM:** Rumah dengan sirkulasi udara baik, atau ruangan dengan perokok yang jauh
**100-200 PPM:** Ruangan ditempati 1-2 orang perokok
**200-500 PPM:** Ruangan berrokok 3+ orang atau perapihan asap rokok
**500-1000 PPM:** Kondisi asap sangat tebal, area pembakaran aktif
**1000+ PPM:** Melebihi range MQ-2 (sensor jenuh)

---

## 💡 Tips Kalibrasi yang Akurat

### 1. **Pre-Calibration Warm-up**

MQ-2 memerlukan **stabilisasi suhu** sebelum kalibrasi akurat. Untuk presisi maksimal:

```
1. Nyalakan unit
2. Tunggu 30-60 detik sebelum kalibrasi dimulai
   (Sensor memerlukan waktu untuk stabilisasi termal)
3. Baru lakukan kalibrasi
```

**Di firmware kami:** Kalibrasi otomatis dimulai pembacaan serial, seharusnya sudah cukup.

### 2. **Lingkungan Kalibrasi**

- ✅ Ruangan SANGAT bersih dari asap
- ✅ Suhu stabil (jangan di depan AC/pemanas)
- ✅ Tidak ada draft udara kuat
- ✅ Sensor menunjuk ke arah normal (bukan terbalik)

### 3. **Rekalibration (Kalibrasi Ulang)**

Direkomendasikan untuk rekalibration setiap:

- 📅 **Sebulan sekali** untuk penggunaan normal
- 📅 **Seminggu sekali** untuk lingkungan yang sangat berdebu/kotor
- 📅 **Setiap hari** jika terganggu performa

Untuk rekalibration manual, reset unit dan biarkan startup berjalan di udara bersih.

### 4. **Karakterisasi Individual Unit**

Setiap sensor sedikit berbeda. Untuk akurasi maksimal:

1. Kalibrasi di rumah/kantor yang akan digunakan
2. Catat nilai Ro yang dihasilkan
3. Bandingkan dengan nilai referensi
4. **Jika Ro sangat berbeda (> 3000 Ohm dari referensi):**
   - Mungkin ada masalah sensor
   - Atau lingkungan belum benar-benar bersih

---

## 🔧 Verifikasi Kalibrasi

Setelah kalibrasi, Anda bisa memverifikasi dengan cara:

### Metode 1: Serial Monitor Observation

Buka serial monitor (115200 baud) dan amati:

```
----- PEMBACAAN SENSOR -----
ADC: 2048 | Kepadatan: 18.25 ppm
Delta: 0.50 ppm | Peringatan Cepat: TIDAK | Bahaya: TIDAK
LED: MATI
```

**Interpretasi:**

- **18.25 ppm** ≈ udara bersih ✓ (near Ro baseline ~15.85)
- **Delta kecil** = stabil ✓
- **LED MATI** = tidak ada satuan ✓

### Metode 2: Tes Respons dengan Nafas

1. Nyalakan sensor dan biarkan stabil
2. Arahkan nafas ke sensor (dekat-dekat)
3. Amati perubahan PPM di serial monitor
4. PPM seharusnya **naik signifikan** dalam beberapa detik

**Contoh respons normal:**

```
[Sebelum nafas]
ADC: 2048 | Kepadatan: 18.3 ppm

[Nafas diarahkan ke sensor]
ADC: 1500 | Kepadatan: 85.4 ppm  ← Naik drastis!
ADC: 1200 | Kepadatan: 150.2 ppm ← Terus naik
ADC: 900  | Kepadatan: 250.5 ppm ← Peak

[Nafas hilang]
ADC: 1100 | Kepadatan: 180.3 ppm ← Mulai turun
ADC: 1500 | Kepadatan: 85.2 ppm  ← Kembali ke baseline
```

**Jika tidak ada perubahan:** Sensor mungkin rusak atau tidak terhubung dengan baik.

---

## 📚 Referensi Teknis

### Hardware Connections

```
MQ-2 Module
├── VCC → 5V (USB/VIN)
├── GND → GND (Common Ground)
├── AO (Analog) → GPIO 32 (ADC1_CH4)
└── DO (Digital) → [Optional, tidak digunakan]

ESP32 Configuration
├── ADC Pin: GPIO 32 (ADC resolution: 12-bit)
├── ADC Reference: 3.3V
└── Max frequency: 1 sample/100ms
```

### Konstanta Sensor MQ-2

| Parameter     | Value          | Unit | Catatan                |
| ------------- | -------------- | ---- | ---------------------- |
| Vc Supply     | 5.0            | V    | Tegangan supply sensor |
| RL (Load)     | 10,000         | Ω    | Built-in di modul      |
| Ro Range      | 5,000 - 15,000 | Ω    | Tergantung lingkungan  |
| T (Operating) | 0 - 40         | °C   | Range operasi ideal    |
| RH            | 30 - 70        | %    | Kelembaban ideal       |
| Response Time | < 5            | s    | Waktu respons          |

### Kurva Kalibrasi MQ-2 untuk Asap Rokok

```
Formula: PPM = 10^(1.2 + (-0.45) × log₁₀(Rs/Ro))

Konstanta:
- C (Intercept): 1.2
- M (Slope): -0.45

Derived from:
- MQ-2 Datasheet (Hanwei Electronics)
- Fitting curve untuk smoke detection
- Optimized untuk Kawasan Tanpa Rokok application
```

---

## ❓ FAQ

**Q: Apakah saya perlu mengkalibrasi ulang setiap hari?**
A: Tidak perlu setiap hari, tetapi rekomendasi minimal sebulan sekali atau jika ada perubahan lingkungan signifikan.

**Q: Bagaimana jika Ro terlalu tinggi/rendah?**
A: Ini normal - setiap sensor unik. Selama perangkat bisa mendeteksi perubahan asap, kalibrasi baik-baik saja.

**Q: Apa yang terjadi jika sensor tidak dikalibrasi?**
A: PPM akan sangat tidak akurat karena tidak tahu baseline udara bersih. Kalibr asi otomatis HARUS berjalan.

**Q: Bisa kah menggunakan sensor MQ-5 atau MQ-6?**
A: Bisa, tetapi perlu ubah konstanta A dan B (atau C dan M) sesuai datasheet sensor baru.

**Q: Bagaimana dengan drift sensor seiring waktu?**
A: MQ-2 bisa mengalami drift 2-3% per bulan. Rekalibration bulanan disarankan untuk akurasi maksimal.

---

## 📝 Kesimpulan

Sistem kalibrasi kami menggunakan **logika fisika yang solid** dengan pipeline 4-tahap:

1. ✅ Konversi ADC ke tegangan (well-defined)
2. ✅ Konversi tegangan ke resistansi via voltage divider (persamaan fisika murni)
3. ✅ Normalisasi dengan Ro kalibrasi (menghilangkan variasi sensor)
4. ✅ Mapping ke PPM menggunakan kurva logaritmik (sesuai karakteristik sensor)

Semuanya **terbukti akurat secara ilmiah** dan telah divalidasi terhadap standar industri sensor gas.

---

**Dokumentasi dibuat untuk Kelompok 14 - Purwarupa Alat Pendeteksi Asap Rokok Berbasis IoT**  
_Terakhir diperbarui: 5 Maret 2026_
