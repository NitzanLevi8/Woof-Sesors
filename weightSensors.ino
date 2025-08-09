#include <WiFi.h>
#include <HTTPClient.h>
#include "HX711.h"

// ========== WIFI Settings ===========
const char* ssid = "goldman11";
const char* password = "102030405060r";

// ========== ID ============
const String DEVICE_ID = "dog123";
const String FIREBASE_URL = "https://woof-gps-default-rtdb.europe-west1.firebasedatabase.app/" + DEVICE_ID + ".json";

// ============ HX711 ============
#define FOOD_DOUT  2
#define FOOD_SCK   3
#define WATER_DOUT 4
#define WATER_SCK  5

HX711 foodScale;
HX711 waterScale;

// ========== Calibration ===========
const float known_weight = 7.0;
float foodCalibration = 1.0;
float waterCalibration = 1.0;
bool calibrated = false;

// ========== Bowl weight offset ===========
const float BOWL_WEIGHT = 60.0;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WIFI");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected");

  foodScale.begin(FOOD_DOUT, FOOD_SCK);
  waterScale.begin(WATER_DOUT, WATER_SCK);
  foodScale.set_scale();
  waterScale.set_scale();
  foodScale.tare();
  waterScale.tare();

  Serial.println("Place 7g (10 shekels) weights on both scales...");
  delay(8000);

  long foodRaw = foodScale.get_units(10);
  long waterRaw = waterScale.get_units(10);
  foodCalibration = (float)foodRaw / known_weight;
  waterCalibration = (float)waterRaw / known_weight;
  foodScale.set_scale(foodCalibration);
  waterScale.set_scale(waterCalibration);
  calibrated = true;
}

void loop() {
  if (!calibrated) return;

  float foodWeight = foodScale.get_units(10) - BOWL_WEIGHT;
  float waterWeight = waterScale.get_units(10) - BOWL_WEIGHT;

//Zero if the result is negative
if (foodWeight < 0) foodWeight = 0;
  if (waterWeight < 0) waterWeight = 0;

  Serial.print("Food: ");
  Serial.print(foodWeight, 2);
  Serial.print(" g | Water: ");
  Serial.print(waterWeight, 2);
  Serial.println(" g");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(FIREBASE_URL);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"food\": {\"weight\": " + String(foodWeight, 2) + "},";
    json += "\"water\": {\"weight\": " + String(waterWeight, 2) + "}";
    json += "}";

    int code = http.PUT(json);
    http.end();

    if (code > 0) {
      Serial.print("Updated Firebase (code ");
      Serial.print(code);
      Serial.println(")");
    } else {
      Serial.print("Failed to send (code ");
      Serial.print(code);
      Serial.println(")");
    }
  }

  delay(5000);
}
