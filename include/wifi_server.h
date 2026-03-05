/**
 * @file wifi_server.h
 * @brief WiFi server implementation for Kelompok 14 - Smoke Detector Prototype
 * 
 * Provides REST API endpoints for device status, user authentication,
 * and air quality index (AQI) monitoring with web dashboard interface.
 * Serves static files (images, CSS) from LittleFS filesystem using SPIFFS.
 * 
 * API Endpoints:
 * - GET  /                  → Serve web interface (index.html)
 * - GET  /status            → Device status (IP, RSSI)
 * - GET  /aqi               → Air quality reading (0-100%)
 * - POST /login             → User authentication
 * - GET  /greflex.jpg       → Logo image 1 from SPIFFS
 * - GET  /gonzaga.jpg       → Logo image 2 from SPIFFS
 * - GET  /<any_static_file> → Serve static files from LittleFS (auto MIME type)
 * 
 * SPIFFS File Structure:
 * ```
 * data/
 * ├── index.html           (Web interface)
 * ├── greflex.jpg          (Logo 1 - must be added to data/ folder)
 * └── gonzaga.jpg          (Logo 2 - must be added to data/ folder)
 * ```
 * 
 * To add logo images:
 * 1. Place greflex.jpg and gonzaga.jpg in the data/ folder
 * 2. Upload filesystem with: platformio run --target uploadfs
 * 3. Access logos at: http://kelompok14.local/greflex.jpg or http://192.168.4.1/greflex.jpg
 */

#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <WebServer.h>
#include <IPAddress.h>
#include <ESPmDNS.h>

/** @brief Global air quality reading from sensor (0-100%) */
extern float airQuality;

/**
 * @struct user_data
 * @brief Stores username and password for device authentication
 * 
 * Used for HTTP Basic Auth on /login endpoint
 */
struct user_data {
    const char* user;  /**< Username for authentication */
    const char* pass;  /**< Password for authentication */
};

/** @brief Array of authorized users for WiFi access control */
const user_data allowedUsers[] = {
    { "Gonzaga", "12345"}
};

/**
 * @class DeviceServer
 * @brief Manages WiFi server and REST API for device monitoring
 * 
 * Handles:
 * - WiFi access point (AP) setup with mDNS support (kelompok14.local)
 * - User authentication against allowed credentials
 * - REST API endpoints for device status and air quality
 * - Static file serving from LittleFS (images, CSS, etc with auto MIME type detection)
 * - Automatic file caching headers for browser optimization
 * 
 * Features:
 * - mDNS: Accessible at http://kelompok14.local from any device on the network
 * - Fallback: Accessible at http://192.168.4.1 if mDNS unavailable
 * - Static files: Served with proper MIME types (.jpg, .png, .css, .js, .svg, .woff, .woff2)
 * - Browser caching: 24-hour cache-control headers for static files
 * 
 * Usage:
 * ```cpp
 * DeviceServer wifi(80);  // Create on port 80
 * wifi.begin();           // Initialize WiFi and HTTP server
 * 
 * // In main loop:
 * wifi.handle();          // Process incoming requests and update mDNS
 * ```
 */
class DeviceServer {
    const char* ssid = "Kelompok 14 - Alat Pendeteksi Asap Rokok";        /**< WiFi SSID (network name) */
    const char* password = "kelompok14";  /**< WiFi password */
    WebServer server;                    /**< Web server instance */
    IPAddress IP;                        /**< Device IP address */
    
    public:
    /**
     * @brief Constructor
     * @param port Server port number (default: 80 for HTTP)
     */
    DeviceServer(uint16_t port);
    
    /**
     * @brief Initialize WiFi and HTTP server
     * 
     * Steps:
     * 1. Mount LittleFS filesystem
     * 2. Start WiFi in AP mode
     * 3. Register API endpoints
     * 4. Start web server
     * 
     * @return true if successful, false on failure
     */
    bool begin();
    
    /**
     * @brief Handle incoming client requests
     * Should be called repeatedly in main loop.
     * Non-blocking call that processes pending HTTP requests.
     */
    void handle();
    
    /** @brief Number of authorized users in allowedUsers array */
    const size_t allowedUserCount = sizeof(allowedUsers) / sizeof(allowedUsers[0]);
    
    /** @brief Destructor - Cleans up resources */
    ~DeviceServer();
};

#endif