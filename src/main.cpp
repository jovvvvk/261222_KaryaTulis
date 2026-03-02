/**
 * @file main.cpp
 * @brief Main firmware for Kelompok 14 - Smoke Detector Prototype
 * 
 * Core functionality:
 * - Reads analog sensor input for smoke/gas detection
 * - Monitors air quality changes for rapid spikes
 * - Controls LED alerts based on detection levels
 * - Provides WiFi connectivity via soft AP
 * - Serves web dashboard for real-time monitoring
 * 
 * Sensor specifications:
 * - Input: ADC (0-4095 raw value)
 * - Output: Percentage (0-100%) gas concentration
 * - Scaling: (raw / 2500.0) * 100 = percentage
 * 
 * Alert thresholds:
 * - QualityLimit = 60%: Triggers danger alert and emission recording
 * - deviationLimit = 5%: Triggers rapid change warning
 */

#include "wifi_server.h"

#define DEBUG  // Enable serial debug output

// ========== PIN ASSIGNMENTS ==========
const int LED_pins = 27;  // GPIO 27: Output pin for alarm LED indicator
const int Sensor_pin = 32;  // GPIO 32: Analog input pin for smoke/gas sensor (ADC)

// ========== ALERT THRESHOLDS ==========
const int QualityLimit = 70;  // Air quality threshold (%) for danger alert
const int deviationLimit = 4;  // Max allowed rapid change between readings (%) - reduced for better responsiveness

// ========== GLOBAL VARIABLES ==========
// WiFi server instance (port 80)
DeviceServer wifi(80);

// Current air quality percentage (0-100)
// Updated by sensor reading in loop and accessed by /aqi endpoint
float airQuality = 0;

/**
 * @brief Arduino setup function - Run once at startup
 * Initializes serial communication, WiFi server, and GPIO pins.
 */
void setup()
{
  // Initialize serial at 115200 baud for debugging and monitoring
  Serial.begin(115200);

#ifdef DEBUG
  Serial.println("\n========================================");
  Serial.println("Kelompok 14 - Smoke Detector Prototype");
  Serial.println("Initializing system...");
  delay(1000);
#endif

  // Start WiFi server
  if (wifi.begin()) {
    Serial.println("[INFO] WiFi server started successfully!");
  } else {
    Serial.println("[ERROR] Failed to start WiFi server!");
  }

  // Configure GPIO pins
  pinMode(LED_pins, OUTPUT);
  pinMode(Sensor_pin, INPUT);
  
  // Initialize LED as OFF
  digitalWrite(LED_pins, LOW);

#ifdef DEBUG
  Serial.println("========================================");
  Serial.println("[INFO] System initialization complete!");
  Serial.println("========================================\n");
#endif
}

/**
 * @brief Arduino loop function - Run repeatedly (target: ~1 second per cycle)
 * 
 * Main control loop responsibilities:
 * 1. Handle WiFi client requests
 * 2. Read analog sensor value
 * 3. Convert raw reading to percentage
 * 4. Calculate delta from previous reading
 * 5. Evaluate alert conditions
 * 6. Control LED output
 * 7. Print debug information
 */
void loop()
{
  // Handle incoming WiFi client requests (non-blocking)
  wifi.handle();
  
  // Track previous sensor value for delta calculation
  static int prev_sensorValue = 0;

  // ========== SENSOR READING ==========
  // Read analog sensor (0-4095 range on ESP32)
  int sensorValue = analogRead(Sensor_pin);

  // Convert sensor reading to air quality percentage (0-100)
  // Formula: (raw_value / 2500.0) * 100.0
  // This scaling maps sensor range to percentage with headroom
  airQuality = (sensorValue / 2500.0f) * 100.0f;
  airQuality = constrain(airQuality, 0.0f, 100.0f);

  // ========== DELTA CALCULATION ==========
  // Calculate change from previous reading for rapid-change detection
  float prev_airQuality = prev_sensorValue ? (prev_sensorValue / 2500.0f) * 100.0f : 0.0f;
  float delta = airQuality - prev_airQuality;

  // ========== ALERT EVALUATION ==========
  // Determine alert conditions
  bool Alert = (prev_sensorValue != 0 && delta > deviationLimit);  // Rapid change alert
  bool Danger = (airQuality > QualityLimit);  // High air quality alert

  // Update previous reading for next cycle
  prev_sensorValue = sensorValue;

#ifdef DEBUG
  // Print debug information to serial for monitoring
  Serial.println("----- SENSOR READING -----");
  Serial.print("Raw Value: ");
  Serial.print(sensorValue);
  Serial.print(" | Quality: ");
  Serial.print(airQuality, 2);
  Serial.println("%");
  
  Serial.print("Delta: ");
  Serial.print(delta, 2);
  Serial.print("% | ");
  
  Serial.print("Alert: ");
  Serial.print(Alert ? "YES" : "NO");
  Serial.print(" | Danger: ");
  Serial.println(Danger ? "YES" : "NO");
  
  Serial.print("LED: ");
  Serial.println((Alert || Danger) ? "ON" : "OFF");
  Serial.println("--------------------------\n");
#endif

  // ========== LED CONTROL ==========
  // Control LED: ON if alert or danger detected, OFF otherwise
  // This provides visual feedback for rapid changes and high levels
  if (!Alert && !Danger)
    digitalWrite(LED_pins, LOW);   // Turn LED OFF
  else
    digitalWrite(LED_pins, HIGH);  // Turn LED ON for alert/danger

  // Sample sensor every 1 second cycle time
  delay(1000);
}