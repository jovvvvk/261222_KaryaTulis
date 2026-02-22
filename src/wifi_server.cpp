#include "wifi_server.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char index_html[] PROGMEM = "";


DeviceServer::DeviceServer(uint16_t port) : server(port) {}
bool DeviceServer::begin() {
    // Initialize LittleFS filesystem
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS!");
        return false;
    }
    
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, password)) return false;
    
    IP = WiFi.softAPIP();
    Serial.print("IP address: ");
    Serial.println(IP);

    server.on("/status", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["ip"] = WiFi.softAPIP().toString();
        doc["rssi"] = WiFi.RSSI();

        char buffer[64];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    server.on("/login", HTTP_POST, [this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, server.arg("plain"));

            if (err) {
                server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            const char* inputSSID = doc["ssid"];
            const char* inputPass = doc["pass"];

            bool authenticated = false;
            for (size_t i = 0; i < allowedUserCount; i++) {
                if (inputSSID && inputPass &&
                    strcmp(inputSSID, allowedUsers[i].user) == 0 &&
                    strcmp(inputPass, allowedUsers[i].pass) == 0) {
                    authenticated = true;
                    break;
                }
            }

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


    server.on("/aqi", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["score"] = airQuality;
        
        char buffer[64];
        serializeJson(doc, buffer);
        server.send(200, "application/json", buffer);
    });

    server.on("/", HTTP_GET, [this]() {
        if (LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            server.streamFile(file, "text/html");
            file.close();
        } else {
            server.send(404, "text/plain", "index.html not found");
        }
    });

    server.begin();
    return true;
}

void DeviceServer::handle() {
    server.handleClient();
}

DeviceServer::~DeviceServer() {}