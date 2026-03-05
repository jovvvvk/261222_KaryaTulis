/**
 * @file main.cpp
 * @brief Firmware utama Kelompok 14 - Purwarupa Alat Pendeteksi Asap Rokok
 * 
 * Fungsionalitas utama:
 * - Membaca masukan sensor analog MQ-2 untuk deteksi asap/gas
 * - Memantau perubahan kepadatan asap untuk lonjakan cepat (rapid change)
 * - Mengendalikan indikator LED berdasarkan tingkat deteksi
 * - Menyediakan konektivitas WiFi melalui soft AP
 * - Melayani dashboard web untuk pemantauan real-time
 * 
 * SISTEM KONVERSI SENSOR (Logika Fisika MQ-2):
 * ============================================
 * Pipeline konversi menggunakan metode standar industri:
 * 1. ADC (0-4095) → Tegangan terukur (0-3.3V)
 * 2. Tegangan → Resistansi Sensor (Rs) melalui voltage divider
 * 3. Rs/Ro → Konsentrasi gas (ppm) menggunakan kurva kalibrasi
 * 
 * Rumus Voltage Divider:
 * Vout = Vin × Rl / (Rl + Rs)
 * Dimana: Vin = 5V (tegangan supply), Rl = 10kΩ (beban resistif)
 * Maka: Rs = Rl × (Vin - Vout) / Vout
 * 
 * Kurva Kalibrasi MQ-2 untuk Asap Rokok:
 * ppm = 10^(1.2 + (-0.45) * log10(Rs/Ro))
 * = 10^1.2 × (Rs/Ro)^(-0.45)
 * = 15.85 × (Rs/Ro)^(-0.45)
 * 
 * Konstanta kalibrasi optimized untuk deteksi asap rokok di lingkungan KTR
 * 
 * Ambang batas peringatan (dalam ppm) - sistem 3 kondisi sederhana:
 * Sumber: Data pengukuran real (ambient 30-40ppm, gas source 100-110ppm)
 * - Aman: 0-35 ppm (udara bersih, tidak ada notifikasi)
 * - Notifikasi: 35-50 ppm (kualitas udara mungkin buruk, notif kuning TANPA pencatatan)
 * - Alert: >50 ppm (asap rokok terdeteksi, pencatatan pelanggaran KTR + LED nyala)
 * - Lonjakan Cepat: perubahan >10 ppm/detik (deteksi anomali, toast notif)
 */

#include "wifi_server.h"
#include <math.h>  // Untuk fungsi pow() dan log10() dalam kurva kalibrasi

#define DEBUG  // Aktifkan keluaran debug serial

// ========== PENUGASAN PIN ==========
/** @brief GPIO 27: Pin keluaran untuk indikator LED alarm */
const int pinLED = 27;

/** @brief GPIO 32: Pin masukan analog untuk sensor asap/gas (ADC) */
const int pinSensor = 32;

// ========== AMBANG BATAS PERINGATAN ==========
/** @brief Ambang batas notifikasi kualitas udara mungkin buruk
 *  Di atas 35 ppm = kualitas mungkin buruk (notif kuning, tanpa pencatatan)
 */
const int batasNotif = 35;

/** @brief Ambang batas alert penuh dan pencatatan pelanggaran
 *  Di atas 50 ppm = alert full + pencatatan pelanggaran KTR
 */
const int batasAlert = 50;

/** @brief Ambang batas perubahan cepat - responsif terhadap lonjakan tiba-tiba
 *  Deviasi maksimal 10 ppm untuk alert anomali
 */
const int batasDeviasiCepat = 10;

// ========== KALIBRASI DAN PARAMETER SENSOR ==========
/** 
 * @brief Resistansi beban (pull-down) pada modul sensor MQ-2
 * Umumnya resistor 10kΩ built-in dalam modul sensor
 * Rumus voltage divider: Vout = Vin × Rl / (Rl + Rsensor)
 */
const float RESISTANSI_BEBAN = 10000.0f;

/** 
 * @brief Tegangan supply untuk sensor MQ-2 
 * Sensor powered dengan 5V dari USB/VIN ESP32
 * CATATAN: Walaupun ADC ESP32 hanya baca 0-3.3V, 
 * Vc tetap 5V karena itu tegangan supply sensor sebenarnya
 */
const float TEGANGAN_SUPPLY = 5.0f;

/** 
 * @brief Resolusi ADC (12-bit pada ESP32)
 * Range: 0-4095 dengan step 3.3V/4095 ≈ 0.8mV per step
 */
const float RESOLUSI_ADC = 4095.0f;

/** 
 * @brief Tegangan referensi ADC pada ESP32 (maksimal yang bisa dibaca)
 * ESP32 ADC menggunakan referensi 3.3V
 */
const float TEGANGAN_REFERENSI = 3.3f;

/**
 * @brief Ro adalah resistansi sensor pada udara bersih (baseline kalibrasi)
 * Nilai ini dikalibrasi otomatis saat startup oleh fungsi kalibrasiBersih()
 * Sensor ditempatkan di udara bersih selama ~5 detik startup
 * MQ-2 typical: Ro berkisar 5000-15000 Ohm tergantung kondisi
 * 
 * Akurasi kalibrasi sangat penting untuk:
 * - Mengurangi drift suhu dan variasi hardware
 * - Mengurangi variasi antar unit sensor (sensor matching)
 * - Meningkatkan akurasi dan repeatability pembacaan ppm
 */
float nilaiRo = 10000.0f;

/** @brief Flag untuk menunjukkan apakah kalibrasi sudah selesai dan sensor siap */
bool sudahKalibrasi = false;

/** @brief Status detil kalibrasi untuk API ("Siap", "Sedang...", "Gagal", dll) */
String statusKalibrasi = "Sedang...";

/**
 * @brief Konstanta kurva kalibrasi MQ-2 untuk deteksi ASAP ROKOK
 * 
 * Formula kalibrasi: ppm = 10^(log_constant - log_slope * log10(Rs/Ro))
 * Atau: ppm = 10^(-0.45 * log10(Rs/Ro) + 1.2)
 * 
 * Konstanta ini dikhususkan untuk deteksi asap (smoke) di ruangan
 * Derived dari datasheet MQ-2 dengan fitting kurva untuk aplikasi KTR
 * 
 * KONSTANTA_C = 1.2 (intercept dari regresi log-log)
 * KONSTANTA_M = -0.45 (slope dari regresi log-log, negatif karena inverse relationship)
 */
const float KONSTANTA_C = 1.2f;   // Log intercept
const float KONSTANTA_M = -0.45f; // Log slope

// ========== VARIABEL GLOBAL ==========
/** @brief Instansi server WiFi (port 80) */
ServerPerangkat wifi(80);

/**
 * @brief Kepadatan asap saat ini dalam satuan ppm (0-1000)
 * Diperbarui oleh pembacaan sensor di loop dan diakses oleh endpoint /kepadatan
 */
float kepadatanAsap = 0;

/**
 * @brief Konversi rasio Rs/Ro ke konsentrasi gas dalam PPM menggunakan kurva MQ-2
 * Formula: ppm = 10^(C + M * log10(Rs/Ro))
 * @param rasioRsRo Rasio resistansi sensor terhadap kalibrasi (Rs/Ro)
 * @return Konsentrasi gas dalam ppm
 */
float rasioKeKepadatan(float rasioRsRo) {
  // Validasi input untuk mencegah NaN dari log10
  if (rasioRsRo <= 0 || isnan(rasioRsRo) || isinf(rasioRsRo)) {
    return 15.85f;  // Return nilai baseline (Ro=Ro, ratio=1.0)
  }
  if (nilaiRo <= 0 || isnan(nilaiRo) || isinf(nilaiRo)) {
    return 0.0f;  // Kalibrasi belum valid
  }
  
  // ppm = 10^(C + M * log10(ratio))
  float logValue = log10(rasioRsRo);
  if (isnan(logValue) || isinf(logValue)) return 15.85f;
  
  float ppm = pow(10.0f, KONSTANTA_C + KONSTANTA_M * logValue);
  if (isnan(ppm) || isinf(ppm)) return 15.85f;
  
  return ppm;
}

/**
 * @brief Konversi nilai ADC (0-4095) ke tegangan (0-3.3V)
 * ADC ESP32 hanya bisa membaca sampai 3.3V (tegangan referensi ADC)
 * @param nilaiADC Nilai pembacaan ADC mentah
 * @return Tegangan dalam Volt
 */
float adcKeTegangan(int nilaiADC) {
  // PENTING: Gunakan float division bukan integer division
  // bila tidak, hasil akan 0 untuk semua nilai < 4095
  return ((float)nilaiADC / RESOLUSI_ADC) * TEGANGAN_REFERENSI;
}

/**
 * @brief Konversi tegangan ke resistansi sensor (Rs) menggunakan voltage divider
 * 
 * Rumus voltage divider:
 * Vout = Vin × Rl / (Rl + Rs)
 * Maka: Rs = Rl × (Vin - Vout) / Vout
 * 
 * Dimana:
 * - Vin = TEGANGAN_SUPPLY (5V, tegangan supply sensor)
 * - Vout = tegangan terukur di pin ADC (0-3.3V)
 * - Rl = RESISTANSI_BEBAN (10kΩ)
 * - Rs = resistansi sensor yang akan dihitung
 * 
 * @param tegangan Tegangan pada pin output sensor (Volt)
 * @return Resistansi sensor dalam Ohm
 */
float teganganKeResistansi(float tegangan) {
  // Validasi input untuk mencegah division by zero atau hasil invalid
  if (tegangan <= 0.01f || tegangan >= TEGANGAN_SUPPLY) return 0;
  if (isnan(tegangan) || isinf(tegangan)) return 0;
  
  // Gunakan TEGANGAN_SUPPLY (5V) bukan TEGANGAN_REFERENSI (3.3V)
  float rs = RESISTANSI_BEBAN * (TEGANGAN_SUPPLY - tegangan) / tegangan;
  
  // Pastikan nilai masuk akal (> 100 Ohm, < 1M Ohm)
  if (rs <= 100 || rs > 1000000 || isnan(rs) || isinf(rs)) return 0;
  
  return rs;
}

/**
 * @brief Konversi nilai ADC langsung ke kepadatan asap (ppm) dengan kalibrasi
 * Pipeline: ADC → Tegangan → Rs → Rs/Ro → ppm
 * @param nilaiADC Nilai pembacaan ADC mentah
 * @return Kepadatan asap dalam ppm (0-1000+ range)
 */
float adcKeKepadatan(int nilaiADC) {
  // Validasi ADC input
  if (nilaiADC < 0 || nilaiADC > 4095) return 0.0f;
  
  // Konversi ADC ke tegangan
  float tegangan = adcKeTegangan(nilaiADC);
  if (isnan(tegangan) || isinf(tegangan)) return 0.0f;
  
  // Konversi tegangan ke resistansi sensor
  float rs = teganganKeResistansi(tegangan);
  if (rs == 0 || isnan(rs) || isinf(rs)) return 0.0f;
  
  // Validasi kalibrasi Ro
  if (nilaiRo <= 0 || isnan(nilaiRo) || isinf(nilaiRo)) return 0.0f;
  
  // Hitung rasio Rs/Ro dengan validasi
  float rasio = rs / nilaiRo;
  if (isnan(rasio) || isinf(rasio) || rasio <= 0) return 15.85f;  // Baseline
  
  // Konversi rasio ke PPM menggunakan kurva kalibrasi
  float ppm = rasioKeKepadatan(rasio);
  
  // Validasi hasil PPM
  if (isnan(ppm) || isinf(ppm)) return 15.85f;  // Baseline value
  
  // Batasi nilai ke range 0-1000 ppm
  return constrain(ppm, 0.0f, 1000.0f);
}

/**
 * @brief Kalibrasi sensor pada udara bersih (menghitung Ro)
 * Harus dipanggil sekali saat startup untuk mendapatkan nilai Ro yang akurat
 * Ambil rata-rata dari beberapa pembacaan
 * @param jumlahSampel Jumlah sampel untuk kalibrasi (default: 50)
 */
void kalibrasiBersih(int jumlahSampel = 50) {
  Serial.println("[INFO] Memulai kalibrasi sensor pada udara bersih...");
  float totalRs = 0.0f;
  
  for (int i = 0; i < jumlahSampel; i++) {
    int nilaiADC = analogRead(pinSensor);
    float tegangan = adcKeTegangan(nilaiADC);
    float rs = teganganKeResistansi(tegangan);
    totalRs += rs;
    
    Serial.print(".");
    delay(100);
  }
  
  // Hitung rata-rata Ro dari sampel
  nilaiRo = totalRs / jumlahSampel;
  sudahKalibrasi = true;
  statusKalibrasi = "Siap";
  
  Serial.println();
  Serial.print("[INFO] Kalibrasi selesai! Ro = ");
  Serial.print(nilaiRo, 2);
  Serial.println(" Ohm");
}

/**
 * @brief Fungsi setup Arduino - Dijalankan sekali saat startup
 * Menginisialisasi komunikasi serial, server WiFi, pin GPIO, dan kalibrasi sensor.
 */
void setup()
{
  // Inisialisasi serial pada 115200 baud untuk debugging dan pemantauan
  Serial.begin(115200);

#ifdef DEBUG
  Serial.println("\n========================================");
  Serial.println("Kelompok 14 - Purwarupa Alat Pendeteksi Asap");
  Serial.println("Menginisialisasi sistem...");
  delay(1000);
#endif

  // Konfigurasi pin GPIO
  pinMode(pinLED, OUTPUT);
  pinMode(pinSensor, INPUT);
  
  // Inisialisasi LED sebagai OFF
  digitalWrite(pinLED, LOW);

  // Lakukan kalibrasi sensor pada udara bersih
  // Pastikan sensor berada di udara bersih saat startup untuk akurasi maksimal
  kalibrasiBersih(50);

  // Memulai server WiFi
  if (wifi.begin()) {
    Serial.println("[INFO] Server WiFi berhasil dimulai!");
  } else {
    Serial.println("[ERROR] Gagal memulai server WiFi!");
  }

#ifdef DEBUG
  Serial.println("========================================");
  Serial.println("[INFO] Inisialisasi sistem selesai!");
  Serial.println("========================================\n");
#endif
}

/**
 * @brief Fungsi loop Arduino - Dijalankan berulang kali (target: ~1 detik per siklus)
 * 
 * Tanggung jawab loop kontrol utama:
 * 1. Menangani permintaan klien WiFi
 * 2. Membaca nilai sensor analog
 * 3. Mengonversi pembacaan ke kepadatan asap (ppm) dengan kalibrasi
 * 4. Menghitung delta dari pembacaan sebelumnya
 * 5. Mengevaluasi kondisi peringatan
 * 6. Mengendalikan keluaran LED
 * 7. Mencetak informasi debug
 */
void loop()
{
  // Menangani permintaan klien WiFi masuk (non-blocking)
  wifi.handle();
  
  // Melacak nilai sensor sebelumnya untuk perhitungan delta
  static float kepadatanSebelumnya = 0.0f;

  // ========== PEMBACAAN SENSOR ==========
  // Membaca sensor analog (range 0-4095 pada ESP32)
  int nilaiADC = analogRead(pinSensor);

  // Konversi pembacaan sensor ke kepadatan asap dalam ppm (0-1000)
  // Pipeline konversi: ADC → Tegangan → Rs → Rs/Ro → PPM
  // Menggunakan formula kurvakalibrasi MQ-sensor: ppm = A × (Rs/Ro)^B
  kepadatanAsap = adcKeKepadatan(nilaiADC);

  // ========== PERHITUNGAN DELTA ==========
  // Hitung perubahan dari pembacaan sebelumnya untuk deteksi perubahan cepat
  float delta = kepadatanAsap - kepadatanSebelumnya;

  // ========== EVALUASI PERINGATAN ==========
  // Tentukan 3 kondisi sederhana:
  // 1. Deviasi: lonjakan >15ppm (peringatan cepat)
  // 2. Notifikasi: >50ppm (kualitas mungkin buruk, notif kuning)
  // 3. Alert: >70ppm (asap terdeteksi, pencatatan KTR)
  
  bool peringatanCepat = (kepadatanSebelumnya > 0 && delta > batasDeviasiCepat);  // Deteksi lonjakan >15ppm
  bool peringatanNotif = (kepadatanAsap > batasNotif);  // Notifikasi: >50ppm (kualitas buruk)
  bool peringatanAlert = (kepadatanAsap > batasAlert);  // Alert penuh: >70ppm (pencatatan)

  // Perbarui pembacaan sebelumnya untuk siklus berikutnya
  kepadatanSebelumnya = kepadatanAsap;

#ifdef DEBUG
  // Cetak informasi debug ke serial untuk pemantauan
  Serial.println("----- PEMBACAAN SENSOR -----");
  Serial.print("ADC: ");
  Serial.print(nilaiADC);
  Serial.print(" | Kepadatan: ");
  Serial.print(kepadatanAsap, 2);
  Serial.println(" ppm");
  
  Serial.print("Delta: ");
  Serial.print(delta, 2);
  Serial.print(" ppm | ");
  Serial.print("Peringatan Cepat: ");
  Serial.print(peringatanCepat ? "YA" : "TIDAK");
  Serial.print(" | Notif Kualitas: ");
  Serial.print(peringatanNotif ? "YA" : "TIDAK");
  Serial.print(" | Alert: ");
  Serial.println(peringatanAlert ? "YA" : "TIDAK");
  
  Serial.print("LED: ");
  Serial.println((peringatanCepat || peringatanAlert) ? "NYALA" : "MATI");
  Serial.println("----------------------------\n");
#endif

  // ========== PENGENDALIAN LED ==========
  // LED NYALA jika: deviasi cepat >15ppm ATAU alert asap >70ppm
  // LED MATI untuk semua kondisi lain (aman <50ppm, notif 50-70ppm)
  if (peringatanAlert || peringatanCepat)
    digitalWrite(pinLED, HIGH);  // Nyalakan LED untuk alert atau lonjakan
  else
    digitalWrite(pinLED, LOW);   // Matikan LED untuk aman atau notif saja

  // Sampling sensor setiap siklus 1 detik
  delay(1000);
}