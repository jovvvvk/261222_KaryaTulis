/**
 * @file wifi_server.h
 * @brief WiFi server implementation for Kelompok 14 - Smoke Detector Prototype
 * 
 * Provides REST API endpoints for device status, user authentication,
 * and air quality index (AQI) monitoring.
 */

#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <WebServer.h>
#include <IPAddress.h>

/** @brief Global air quality reading from sensor */
extern float airQuality;

/**
 * @struct user_data
 * @brief Stores username and password for device authentication
 */
struct user_data {
    const char* user;  /**< Username */
    const char* pass;  /**< Password */
};

/** @brief Array of authorized users for WiFi access control */
const  user_data allowedUsers[] = {
    { "Jov",            "pass" },
    { "Kelompok14_Guest", "guest123" },
    { "p",              "getacc" },
    { "UserX",          "abcX" }
};

/**
 * @class DeviceServer
 * @brief Manages WiFi server and REST API for Kelompok 14 - Smoke Detector Prototype
 * 
 * Handles WiFi access point setup, user authentication, and API endpoints
 * for device status and air quality monitoring.
 */
class DeviceServer {
    const char* ssid = "myESP32";  /**< WiFi SSID */
    const char* password = "esp32wifi";  /**< WiFi password */
    WebServer server;  /**< Web server instance */
    IPAddress IP;  /**< Device IP address */
    
    public:
    /**
     * @brief Constructor
     * @param port Server port number
     */
    DeviceServer(uint16_t port);
    
    /**
     * @brief Initialize WiFi and HTTP server
     * @return true if successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Handle incoming client requests
     */
    void handle();
    
    /** @brief Number of authorized users */
    const size_t allowedUserCount = sizeof(allowedUsers) / sizeof(allowedUsers[0]);
    
    /** @brief Destructor */
    ~DeviceServer();
};

#endif