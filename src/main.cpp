/**
 * @file main.cpp
 * @brief Main firmware for Kelompok 14 - Smoke Detector Prototype
 * 
 * Reads analog sensor input, monitors air quality changes,
 * controls LED alerts, and provides WiFi connectivity.
 */

#include "wifi_server.h"

#define DEBUG  // Enable serial debug output

// Pin assignments
const int LED_pins = 26;  // Output pin for alarm LED
const int Sensor_pin = 32;  // Analog input pin for smoke sensor

// Alert thresholds
const int QualityLimit = 60;  // Air quality threshold for danger alert
const int deviationLimit = 10;  // Max allowed change between readings

// WiFi server instance (port 80)
DeviceServer wifi(80);

// Current air quality percentage (0-100)
float airQuality = 0;

/**
 * @brief Arduino setup function - Run once at startup
 * Initializes serial communication, WiFi server, and GPIO pins.
 */
void setup()
{
  // Initialize serial at 115200 baud for debugging
  Serial.begin(115200);

#ifdef DEBUG
  Serial.println("Initializing Smoke Detector...");
  delay(1000);
  Serial.println("Initialization Complete.");
  Serial.println("running...");
#endif

  // Start WiFi and report status
  (wifi.begin()) ? Serial.println("WiFi started successfully!") : Serial.println("Failed to start WiFi!");

  // Configure GPIO pins
  pinMode(LED_pins, OUTPUT);
  pinMode(Sensor_pin, INPUT);
}

/**
 * @brief Arduino loop function - Run repeatedly
 * Polls sensor, calculates air quality, and controls LED alert.
 */
void loop()
{
  // Handle WiFi client requests
  wifi.handle();
  
  // Track sensor value between readings
  static int prev_sensorValue = 0;

  // Read analog sensor (0-4095 range)
  int sensorValue = analogRead(Sensor_pin);

  // Convert sensor reading to air quality percentage (0-100)
  airQuality = (sensorValue / 2500.0f) * 100.0f;
  airQuality = constrain(airQuality, 0.0f, 100.0f);

  // Calculate change from previous reading
  float prev_airQuality = prev_sensorValue ? (prev_sensorValue / 2500.0f) * 100.0f : 0.0f;
  float delta = airQuality - prev_airQuality;

  // Determine alert conditions
  bool Alert = (prev_sensorValue != 0 && delta > deviationLimit);  // Rapid change alert
  bool Danger = (airQuality > QualityLimit);  // High air quality alert

  // Update previous reading
  prev_sensorValue = sensorValue;

#ifdef DEBUG
  // Print debug information to serial
  Serial.println("-----------------------");
  Serial.print("Sensor Value: ");
  Serial.println(sensorValue);
  Serial.print("Delta: ");
  Serial.println(delta);
  (Alert || Danger) ? Serial.println("LED ON") : Serial.println("LED OFF");
  Serial.println("-----------------------\n");
#endif

  // Control LED: ON if alert or danger detected, OFF otherwise
  if (!Alert && !Danger)
    digitalWrite(LED_pins, LOW);
  else
    digitalWrite(LED_pins, HIGH);

  // Sample sensor every 1 second
  delay(1000);
}