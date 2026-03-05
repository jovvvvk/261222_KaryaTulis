/**
 * @file wifi_server.cpp
 * @brief Implementasi server WiFi untuk Kelompok 14 - Purwarupa Alat Pendeteksi Asap Rokok
 * 
 * Menyediakan endpoint REST API untuk:
 * - Status perangkat (/status)
 * - Autentikasi pengguna (/login)
 * - Pemantauan kepadatan asap (/kepadatan)
 * - Melayani antarmuka web (/)
 * 
 * Menggunakan LittleFS untuk melayani file web statis dan ArduinoJson untuk serialisasi JSON.
 * Semua endpoint didokumentasikan dengan format permintaan/respons yang diharapkan.
 */

#include "wifi_server.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Variabel eksternal untuk pembacaan kepadatan asap dari main.cpp
extern float kepadatanAsap;

/** @brief Variabel eksternal untuk status kalibrasi dari main.cpp */
extern String statusKalibrasi;

/** @brief Variabel eksternal untuk nilai Ro (resistansi sensor di udara bersih) dari main.cpp */
extern float nilaiRo;

/** @brief Variabel eksternal untuk flag kalibrasi selesai dari main.cpp */
extern bool sudahKalibrasi;

/**
 * @brief Konstruktor - Inisialisasi server dengan nomor port
 * @param port Port server HTTP (default: 80)
 */
ServerPerangkat::ServerPerangkat(uint16_t port) : server(port) {}

/**
 * @brief Inisialisasi titik akses WiFi dan endpoint HTTP
 * Menyiapkan WiFi AP, menginisialisasi LittleFS, dan mendaftarkan semua endpoint API.
 * @return true jika penyiapan berhasil, false jika gagal
 */
bool ServerPerangkat::begin() {
    // Inisialisasi filesystem LittleFS untuk file web
    if (!LittleFS.begin()) {
        Serial.println("[ERROR] Gagal mount LittleFS!");
        return false;
    }
    
    // Mulai WiFi dalam mode Access Point
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, kataSandi)) {
        Serial.println("[ERROR] Gagal memulai WiFi AP!");
        return false;
    }
    
    // Simpan dan laporkan alamat IP
    IP = WiFi.softAPIP();
    Serial.print("[INFO] WiFi AP dimulai. Alamat IP: ");
    Serial.println(IP);

    // Inisialisasi mDNS dengan hostname "kelompok14"
    // Pengguna dapat mengakses perangkat di http://kelompok14.local
    if (!MDNS.begin("kelompok14")) {
        Serial.println("[WARNING] Inisialisasi mDNS gagal!");
        Serial.println("[INFO] Perangkat tetap dapat diakses via http://192.168.4.1");
    } else {
        Serial.println("[INFO] Responder mDNS dimulai. Akses di: http://kelompok14.local");
        // Tambahkan deskripsi layanan (opsional)
        MDNS.addService("http", "tcp", 80);
    }

    // ========== ENDPOINT: /status (GET) ==========
    /**
     * Mengembalikan informasi status perangkat
     * Respons: {"ip": "192.168.4.1", "rssi": -45}
     */
    server.on("/status", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["ip"] = WiFi.softAPIP().toString();
        doc["rssi"] = WiFi.RSSI();

        char buffer[64];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    // ========== ENDPOINT: /login (POST) ==========
    /**
     * Mengautentikasi kredensial pengguna terhadap daftar penggunaTerauthorisasi
     * Permintaan: {"ssid": "nama_pengguna", "pass": "kata_sandi"}
     * Respons: {"success": true} atau {"success": false}
     */
    server.on("/login", HTTP_POST, [this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, server.arg("plain"));

            // Kembalikan error jika JSON tidak valid
            if (err) {
                Serial.println("[ERROR] JSON tidak valid dalam permintaan login");
                server.send(400, "application/json", "{\"error\":\"JSON Tidak Valid\"}");
                return;
            }

            // Ekstrak kredensial dari payload JSON
            const char* inputSSID = doc["ssid"];
            const char* inputPass = doc["pass"];

            // Periksa kredensial terhadap pengguna yang diizinkan
            bool terautentikasi = false;
            for (size_t i = 0; i < jumlahPenggunaTerauthorisasi; i++) {
                if (inputSSID && inputPass &&
                    strcmp(inputSSID, penggunaTerauthorisasi[i].pengguna) == 0 &&
                    strcmp(inputPass, penggunaTerauthorisasi[i].kataSandi) == 0) {
                    terautentikasi = true;
                    Serial.print("[INFO] Pengguna terautentikasi: ");
                    Serial.println(inputSSID);
                    break;
                }
            }

            // Kembalikan hasil autentikasi
            server.send(
                200,
                "application/json",
                terautentikasi ? "{\"success\":true}" : "{\"success\":false}"
            );
        } 
        else {
            Serial.println("[ERROR] Tidak ada body dalam permintaan login");
            server.send(400, "application/json", "{\"error\":\"Permintaan Buruk\"}");
        }
    });

    // ========== ENDPOINT: /kepadatan (GET) ==========
    /**
     * Mengembalikan skor kepadatan asap saat ini
     * Respons: {"score": 450.5}
     * Range score: 0-1000 (0=bersih, 1000=berbahaya)
     */
    server.on("/kepadatan", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["score"] = kepadatanAsap;
        
        char buffer[64];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    // ========== ENDPOINT: /status-kalibrasi (GET) ==========
    /**
     * Mengembalikan status kalibrasi sensor
     * Respons: {"status": "Siap", "ro": 10234.56, "calibrated": true}
     * status: "Sedang...", "Siap", "Gagal", dll
     * ro: Nilai resistansi baseline di udara bersih (Ohm)
     * calibrated: Flag boolean apakah kalibrasi sudah selesai
     */
    server.on("/status-kalibrasi", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["status"] = statusKalibrasi;
        doc["ro"] = nilaiRo;
        doc["calibrated"] = sudahKalibrasi;
        
        char buffer[128];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    // ========== ENDPOINT: / (GET) ==========
    /**
     * Melayani antarmuka web (index.html) dari filesystem LittleFS
     * Menampilkan dashboard responsif untuk pemantauan kepadatan asap
     */
    server.on("/", HTTP_GET, [this]() {
        if (LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            server.streamFile(file, "text/html");
            file.close();
        } else {
            Serial.println("[ERROR] index.html tidak ditemukan di LittleFS");
            server.send(404, "text/plain", "index.html tidak ditemukan");
        }
    });

    // ========== PELAYANAN FILE STATIS (Gambar, CSS, dll) ==========
    /**
     * Melayani file statis dari filesystem LittleFS
     * Mendukung: format .jpg, .png, .gif, .css, .js, .woff, .woff2, .svg
     * Secara otomatis mendeteksi MIME type berdasarkan ekstensi file
     */
    // Layani file statis (js, jpg, css, dll.) dari LittleFS
        server.onNotFound([this]() {
        String path = server.uri();

            if (LittleFS.exists(path)) {
            File file = LittleFS.open(path, "r");

            String contentType = "text/plain";
            if (path.endsWith(".html"))                contentType = "text/html";
            else if (path.endsWith(".js"))             contentType = "application/javascript";
            else if (path.endsWith(".css"))            contentType = "text/css";
            else if (path.endsWith(".jpg") ||
                    path.endsWith(".jpeg"))           contentType = "image/jpeg";
            else if (path.endsWith(".png"))            contentType = "image/png";

            server.streamFile(file, contentType);
            file.close();
            return;}

    Serial.print("[WARN] File tidak ditemukan: ");
    Serial.println(path);
    server.send(404, "application/json", "{\"error\":\"Tidak Ditemukan\"}");
});

    // Mulai web server
    server.begin();
    Serial.println("[INFO] Server HTTP dimulai di port 80");
    return true;
}

/**
 * @brief Proses permintaan HTTP masuk
 * Harus dipanggil berulang kali di main loop untuk menangani permintaan klien.
 * Pemanggilan non-blocking yang memproses permintaan yang tertunda.
 */
void ServerPerangkat::handle() {
    server.handleClient();
}

/**
 * @brief Destruktor
 * Membersihkan resources server (jika diperlukan)
 */
ServerPerangkat::~ServerPerangkat() {}