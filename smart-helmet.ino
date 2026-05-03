/*
Project: IoT Smart Helmet
Author: Kishore Kanna P
Description: Monitors gas, fall detection, and helmet usage using ESP8266 and sensors.
*/

#include "config.h"
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <Arduino.h>
#include <math.h>

#define BLYNK_TEMPLATE_ID "TMPLxxxx"
#define BLYNK_TEMPLATE_NAME "IOT Based Smart Helmet"
#define BLYNK_AUTH_TOKEN "your_real_token"

char ssid[] = "your_wifi";
char pass[] = "your_password";

// Sensor Pins
#define MQ135_PIN A0
#define IR_SENSOR_PIN 12
#define GAS_THRESHOLD 100

uint8_t MPU_ADDR = 0;
const unsigned long loopInterval = 1000;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(50);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  Wire.begin(4, 5); // SDA=D2, SCL=D1
  delay(100);

  Serial.println("\n=== Smart Helmet Starting ===");

  // Detect MPU6050
  for (uint8_t addr = 0x68; addr <= 0x69; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      MPU_ADDR = addr;
      Serial.print("MPU6050 found at 0x");
      Serial.println(MPU_ADDR, HEX);
      break;
    }
  }

  if (MPU_ADDR != 0) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0); // Wake up MPU6050
    Wire.endTransmission(true);

    Serial.println("MPU6050 ready!");
  } else {
    Serial.println("MPU6050 NOT found!");
  }

  Serial.println("Setup Complete.\n");
}

// ---------------- MQ135 ----------------
float readMQ135(int samples = 8) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(MQ135_PIN);
    delay(10);
  }
  return sum / (float)samples;
}

// ---------------- MPU6050 ----------------
bool readMPU6050(float &ax, float &ay, float &az) {
  if (MPU_ADDR == 0) return false;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom(MPU_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;

  int16_t AcX = (Wire.read() << 8) | Wire.read();
  int16_t AcY = (Wire.read() << 8) | Wire.read();
  int16_t AcZ = (Wire.read() << 8) | Wire.read();

  ax = AcX / 16384.0;
  ay = AcY / 16384.0;
  az = AcZ / 16384.0;

  return true;
}

// ---------------- IR ----------------
int readIR() {
  return digitalRead(IR_SENSOR_PIN);
}

// ---------------- LOOP ----------------
void loop() {
  Blynk.run();

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < loopInterval) return;
  lastUpdate = millis();

  // ---- MQ135 ----
  float gasValue = readMQ135();
  String gasMsg = (gasValue > GAS_THRESHOLD) ? "⚠ TOXIC GAS DETECTED" : "✅ Normal Air";

  Serial.print("[GAS] ");
  Serial.println(gasMsg);

  Blynk.virtualWrite(V1, gasValue);
  Blynk.virtualWrite(V2, gasMsg);

  if (gasValue > GAS_THRESHOLD) {
    Blynk.logEvent("gas_alert");
  }

  // ---- MPU6050 ----
  float ax = 0, ay = 0, az = 0;

  if (readMPU6050(ax, ay, az)) {
    Blynk.virtualWrite(V3, ax);
    Blynk.virtualWrite(V4, ay);
    Blynk.virtualWrite(V5, az);

    // FIXED LOGIC
    String dangerMsg = (fabsf(az) < 0.3f) ? "⚠ DANGER! Possible Fall" : "✅ Safe";

    Blynk.virtualWrite(V6, dangerMsg);
    Serial.println(dangerMsg);

    if (fabsf(az) < 0.3f) {
      Blynk.logEvent("fall_alert");
    }
  }

  // ---- IR Helmet ----
  int irValue = readIR();
  String helmetMsg = (irValue == LOW) ? "✅ Helmet WORN" : "❌ Helmet NOT WORN";

  Serial.println(helmetMsg);
  Blynk.virtualWrite(V0, helmetMsg);

  if (irValue != LOW) {
    Blynk.logEvent("helmet_alert");
  }

  Serial.println(" ");
}
