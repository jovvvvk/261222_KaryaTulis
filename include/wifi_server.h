/**
 * @file wifi_server.h
 * @brief Implementasi server WiFi Kelompok 14 - Purwarupa Alat Pendeteksi Asap Rokok
 * 
 * Menyediakan endpoint REST API untuk status perangkat, autentikasi pengguna,
 * dan pemantauan indeks kepadatan asap (dalam satuan ppm) dengan antarmuka dashboard web.
 * Melayani file statis (gambar, CSS) dari filesystem LittleFS menggunakan SPIFFS.
 * 
 * Endpoint API:
 * - GET  /                  → Melayani antarmuka web (index.html)
 * - GET  /status            → Status perangkat (IP, RSSI)
 * - GET  /kepadatan         → Pembacaan kepadatan asap (0-1000 ppm)
 * - POST /login             → Autentikasi pengguna
 * - GET  /greflex.jpg       → Gambar logo 1 dari LittleFS
 * - GET  /gonzaga.jpg       → Gambar logo 2 dari LittleFS
 * - GET  /<berkas_statis>   → Melayani file statis dari LittleFS (MIME type otomatis)
 * 
 * Struktur File LittleFS:
 * ```
 * data/
 * ├── index.html           (Antarmuka web)
 * ├── greflex.jpg          (Logo 1 - harus ditambahkan ke folder data/)
 * └── gonzaga.jpg          (Logo 2 - harus ditambahkan ke folder data/)
 * ```
 * 
 * Untuk menambahkan gambar logo:
 * 1. Letakkan greflex.jpg dan gonzaga.jpg di folder data/
 * 2. Upload filesystem dengan: platformio run --target uploadfs
 * 3. Akses logo di: http://kelompok14.local/greflex.jpg atau http://192.168.4.1/greflex.jpg
 */

#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <WebServer.h>
#include <IPAddress.h>
#include <ESPmDNS.h>

/** @brief Pembacaan kepadatan asap global dari sensor (0-1000 ppm) */
extern float kepadatanAsap;

/**
 * @struct data_pengguna
 * @brief Menyimpan nama pengguna dan kata sandi untuk autentikasi perangkat
 * 
 * Digunakan untuk HTTP Basic Auth pada endpoint /login
 */
struct data_pengguna {
    const char* pengguna;  /**< Nama pengguna untuk autentikasi */
    const char* kataSandi;  /**< Kata sandi untuk autentikasi */
};

/** @brief Daftar pengguna yang diizinkan untuk kontrol akses WiFi */
const data_pengguna penggunaTerauthorisasi[] = {
    { "Gonzaga", "12345"}
};

/**
 * @class ServerPerangkat
 * @brief Mengelola server WiFi dan REST API untuk pemantauan perangkat
 * 
 * Menangani:
 * - Penyiapan titik akses WiFi (AP) dengan dukungan mDNS (kelompok14.local)
 * - Autentikasi pengguna terhadap kredensial yang diizinkan
 * - Endpoint REST API untuk status perangkat dan kepadatan asap
 * - Melayani file statis dari LittleFS (gambar, CSS, dll dengan deteksi MIME type otomatis)
 * - Header cache file otomatis untuk optimasi browser
 * 
 * Fitur:
 * - mDNS: Dapat diakses di http://kelompok14.local dari perangkat manapun di jaringan
 * - Fallback: Dapat diakses di http://192.168.4.1 jika mDNS tidak tersedia
 * - File statis: Dilayani dengan MIME type yang tepat (.jpg, .png, .css, .js, .svg, .woff, .woff2)
 * - Cache browser: Header cache-control 24-jam untuk file statis
 * 
 * Penggunaan:
 * ```cpp
 * ServerPerangkat wifi(80);  // Buat di port 80
 * wifi.begin();              // Inisialisasi WiFi dan server HTTP
 * 
 * // Di main loop:
 * wifi.handle();             // Proses permintaan masuk dan perbarui mDNS
 * ```
 */
class ServerPerangkat {
    const char* ssid = "Kelompok 14 - Alat Pendeteksi Asap Rokok";        /**< SSID WiFi (nama jaringan) */
    const char* kataSandi = "kelompok14";  /**< Kata sandi WiFi */
    WebServer server;                    /**< Instansi web server */
    IPAddress IP;                        /**< Alamat IP perangkat */
    
    public:
    /**
     * @brief Konstruktor
     * @param port Nomor port server (default: 80 untuk HTTP)
     */
    ServerPerangkat(uint16_t port);
    
    /**
     * @brief Inisialisasi WiFi dan server HTTP
     * 
     * Langkah-langkah:
     * 1. Mount filesystem LittleFS
     * 2. Mulai WiFi dalam mode AP
     * 3. Daftarkan endpoint API
     * 4. Mulai web server
     * 
     * @return true jika sukses, false jika gagal
     */
    bool begin();
    
    /**
     * @brief Tangani permintaan klien masuk
     * Harus dipanggil berulang kali di main loop.
     * Pemanggilan non-blocking yang memproses permintaan HTTP yang tertunda.
     */
    void handle();
    
    /** @brief Jumlah pengguna yang diizinkan dalam daftar penggunaTerauthorisasi */
    const size_t jumlahPenggunaTerauthorisasi = sizeof(penggunaTerauthorisasi) / sizeof(penggunaTerauthorisasi[0]);
    
    /** @brief Destruktor - Membersihkan resources */
    ~ServerPerangkat();
};

#endif