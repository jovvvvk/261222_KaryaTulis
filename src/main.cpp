#include "wifi_server.h"

#define DEBUG

const int LED_pins = 26;
const int Sensor_pin = 32;

const int QualityLimit = 60;
const int deviationLimit = 10;

DeviceServer wifi(80);

float airQuality = 0;

void setup()
{
  Serial.begin(115200);

#ifdef DEBUG
  Serial.println("Initializing Smoke Detector...");
  delay(1000);
  Serial.println("Initialization Complete.");
  Serial.println("running...");
#endif

  (wifi.begin()) ? Serial.println("WiFi started successfully!") : Serial.println("Failed to start WiFi!");

  pinMode(LED_pins, OUTPUT);
  pinMode(Sensor_pin, INPUT);
}

void loop()
{
  wifi.handle();
  static int prev_sensorValue = 0;

  int sensorValue = analogRead(Sensor_pin);

  airQuality = (sensorValue / 2500.0f) * 100.0f;
  airQuality = constrain(airQuality, 0.0f, 100.0f);

  float prev_airQuality = prev_sensorValue ? (prev_sensorValue / 2500.0f) * 100.0f : 0.0f;
  float delta = airQuality - prev_airQuality;

  bool Alert = (prev_sensorValue != 0 && delta > deviationLimit);
  bool Danger = (airQuality > QualityLimit);

  prev_sensorValue = sensorValue;

#ifdef DEBUG
  Serial.println("-----------------------");
  Serial.print("Sensor Value: ");
  Serial.println(sensorValue);
  Serial.print("Delta: ");
  Serial.println(delta);
  (Alert || Danger) ? Serial.println("LED ON") : Serial.println("LED OFF");
  Serial.println("-----------------------\n");
#endif

  if (!Alert && !Danger)
    digitalWrite(LED_pins, LOW);
  else
    digitalWrite(LED_pins, HIGH);

  delay(1000);
}