/**
 * @file wifi_server.cpp
 * @brief WiFi server implementation for Kelompok 14 - Smoke Detector Prototype
 * 
 * Provides REST API endpoints for:
 * - Device status (/status)
 * - User authentication (/login)
 * - Air quality monitoring (/aqi)
 * - Web interface serving (/)
 * 
 * Uses LittleFS for serving static web files and ArduinoJson for JSON serialization.
 * All endpoints are documented with their expected request/response formats.
 */

#include "wifi_server.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// External variable for air quality reading from main.cpp
extern float airQuality;

/**
 * @brief Constructor - Initialize server with port number
 * @param port HTTP server port (default: 80)
 */
DeviceServer::DeviceServer(uint16_t port) : server(port) {}

/**
 * @brief Initialize WiFi access point and HTTP endpoints
 * Sets up WiFi AP, initializes LittleFS, and registers all API endpoints.
 * @return true if setup successful, false on failure
 */
bool DeviceServer::begin() {
    // Initialize LittleFS filesystem for web files
    if (!LittleFS.begin()) {
        Serial.println("[ERROR] Failed to mount LittleFS!");
        return false;
    }
    
    // Start WiFi in Access Point mode
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, password)) {
        Serial.println("[ERROR] Failed to start WiFi AP!");
        return false;
    }
    
    // Store and report IP address
    IP = WiFi.softAPIP();
    Serial.print("[INFO] WiFi AP started. IP address: ");
    Serial.println(IP);

    // ========== ENDPOINT: /status (GET) ==========
    /**
     * Returns device status information
     * Response: {"ip": "192.168.4.1", "rssi": -45}
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
     * Authenticates user credentials against allowedUsers array
     * Request: {"ssid": "username", "pass": "password"}
     * Response: {"success": true} or {"success": false}
     */
    server.on("/login", HTTP_POST, [this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, server.arg("plain"));

            // Return error if JSON is malformed
            if (err) {
                Serial.println("[ERROR] Invalid JSON in login request");
                server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            // Extract credentials from JSON payload
            const char* inputSSID = doc["ssid"];
            const char* inputPass = doc["pass"];

            // Check credentials against authorized users
            bool authenticated = false;
            for (size_t i = 0; i < allowedUserCount; i++) {
                if (inputSSID && inputPass &&
                    strcmp(inputSSID, allowedUsers[i].user) == 0 &&
                    strcmp(inputPass, allowedUsers[i].pass) == 0) {
                    authenticated = true;
                    Serial.print("[INFO] User authenticated: ");
                    Serial.println(inputSSID);
                    break;
                }
            }

            // Return authentication result
            server.send(
                200,
                "application/json",
                authenticated ? "{\"success\":true}" : "{\"success\":false}"
            );
        } 
        else {
            Serial.println("[ERROR] No body in login request");
            server.send(400, "application/json", "{\"error\":\"Bad Request\"}");
        }
    });

    // ========== ENDPOINT: /aqi (GET) ==========
    /**
     * Returns current air quality index score
     * Response: {"score": 45.5}
     * Score range: 0-100 (0=clean, 100=dangerous)
     */
    server.on("/aqi", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["score"] = airQuality;
        
        char buffer[64];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    // ========== ENDPOINT: / (GET) ==========
    /**
     * Serves web interface (index.html) from LittleFS filesystem
     * Displays responsive dashboard for air quality monitoring
     */
    server.on("/", HTTP_GET, [this]() {
        if (LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            server.streamFile(file, "text/html");
            file.close();
        } else {
            Serial.println("[ERROR] index.html not found in LittleFS");
            server.send(404, "text/plain", "index.html not found");
        }
    });

    // Handle 404 Not Found
    server.onNotFound([this]() {
        Serial.print("[WARN] Unknown endpoint requested: ");
        Serial.println(server.uri());
        server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
    });

    // Start the web server
    server.begin();
    Serial.println("[INFO] HTTP server started on port 80");
    return true;
}

/**
 * @brief Process incoming HTTP requests
 * Should be called repeatedly in main loop to handle client requests.
 * Non-blocking call that processes pending requests.
 */
void DeviceServer::handle() {
    server.handleClient();
}

/**
 * @brief Destructor
 * Cleans up server resources (if needed)
 */
DeviceServer::~DeviceServer() {}