/**
 * @file wifi_server.cpp
 * @brief WiFi server implementation for Kelompok 14 - Smoke Detector Prototype
 * 
 * Provides REST API endpoints for:
 * - Device status (/status)
 * - User authentication (/login)
 * - Air quality monitoring (/aqi)
 * - Web interface serving (/)
 */

#include "wifi_server.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Placeholder for HTML content loaded from SPIFFS
static const char index_html[] PROGMEM = "";


/**
 * @brief Constructor - Initialize server with port number
 * @param port HTTP server port (default: 80)
 */
DeviceServer::DeviceServer(uint16_t port) : server(port) {}

/**
 * @brief Initialize WiFi access point and HTTP endpoints
 * @return true if setup successful, false on failure
 */
bool DeviceServer::begin() {
    // Initialize LittleFS filesystem for web files
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS!");
        return false;
    }
    
    // Start WiFi in Access Point mode
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, password)) return false;
    
    // Store and report IP address
    IP = WiFi.softAPIP();
    Serial.print("IP address: ");
    Serial.println(IP);

    // Endpoint: /status (GET)
    // Returns device IP and WiFi signal strength
    server.on("/status", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["ip"] = WiFi.softAPIP().toString();
        doc["rssi"] = WiFi.RSSI();

        char buffer[64];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    // Endpoint: /login (POST)
    // Authenticates user credentials against allowedUsers array
    // Expects JSON: {"ssid": "username", "pass": "password"}
    server.on("/login", HTTP_POST, [this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, server.arg("plain"));

            // Return error if JSON is malformed
            if (err) {
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
            server.send(400, "application/json", "{\"error\":\"Bad Request\"}");
        }
    });


    // Endpoint: /aqi (GET)
    // Returns current air quality index score (0-100)
    server.on("/aqi", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["score"] = airQuality;
        
        char buffer[64];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    // Endpoint: / (GET)
    // Serves web interface from LittleFS filesystem
    server.on("/", HTTP_GET, [this]() {
        if (LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            server.streamFile(file, "text/html");
            file.close();
        } else {
            server.send(404, "text/plain", "index.html not found");
        }
    });

    // Start the web server
    server.begin();
    return true;
}

/**
 * @brief Process incoming HTTP requests
 * Call this repeatedly in main loop to handle client requests.
 */
void DeviceServer::handle() {
    server.handleClient();
}

/**
 * @brief Destructor
 */
DeviceServer::~DeviceServer() {}