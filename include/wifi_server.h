/**
 * @file wifi_server.h
 * @brief WiFi server implementation for Kelompok 14 - Smoke Detector Prototype
 * 
 * Provides REST API endpoints for device status, user authentication,
 * and air quality index (AQI) monitoring with web dashboard interface.
 * 
 * API Endpoints:
 * - GET  /          → Serve web interface (index.html)
 * - GET  /status    → Device status (IP, RSSI)
 * - GET  /aqi       → Air quality reading (0-100%)
 * - POST /login     → User authentication
 */

#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <WebServer.h>
#include <IPAddress.h>

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
    { "Jov",            "pass" },
    { "Kelompok14_Guest", "guest123" },
    { "p",              "getacc" },
    { "UserX",          "abcX" }
};

/**
 * @class DeviceServer
 * @brief Manages WiFi server and REST API for device monitoring
 * 
 * Handles:
 * - WiFi access point setup and configuration
 * - User authentication against allowed credentials
 * - REST API endpoints for device status and air quality
 * - Static file serving (web dashboard)
 * 
 * Usage:
 * ```cpp
 * DeviceServer wifi(80);  // Create on port 80
 * wifi.begin();           // Initialize and start server
 * 
 * // In main loop:
 * wifi.handle();          // Process incoming requests
 * ```
 */
class DeviceServer {
    const char* ssid = "myESP32";        /**< WiFi SSID (network name) */
    const char* password = "esp32wifi";  /**< WiFi password */
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