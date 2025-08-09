#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "HX711.h"
#include <Preferences.h>

// ========== WIFI ==========
const char* ssid     = ""; //TODO
const char* password = ""; //TODO

// ========== Firebase ==========
const String DEVICE_ID    = "dog123";
const String FIREBASE_URL = "https://woof-gps-default-rtdb.europe-west1.firebasedatabase.app/" + DEVICE_ID + ".json";

// ========== HX711 Pins ==========
#define FOOD_DOUT   2
#define FOOD_SCK    3
#define WATER_DOUT  4
#define WATER_SCK   5

HX711 foodScale;
HX711 waterScale;

// ========== Calibration ==========
const float KNOWN_WEIGHT_G = 60.0;
const int   CAL_AVG        = 40;   
const int   LOOP_AVG       = 15;    
bool calibrated = false;
double foodCal  = 0.0;
double waterCal = 0.0;

Preferences prefs;
WiFiClientSecure secure;

// ------------ Helpers ------------
bool waitReady(HX711& s, uint32_t ms = 2500) {
  uint32_t t0 = millis();
  while (!s.is_ready()) {
    if (millis() - t0 > ms) return false;
    delay(5);
  }
  return true;
}

double calibrateOne(HX711& s, const char* name) {
  Serial.printf("[%s] Tare (EMPTY bowl ON sensor)...\n", name);
  waitReady(s); s.tare(CAL_AVG);

  Serial.printf("[%s] Place %.1f g reference and keep it steady...\n", name, KNOWN_WEIGHT_G);
  delay(3000);
  waitReady(s);
  long raw = s.get_value(CAL_AVG);
  double factor = (double)raw / KNOWN_WEIGHT_G;
  if (fabs(factor) < 1.0) {
    Serial.printf("[%s] WARNING: tiny delta (%ld). Check wiring/weight.\n", name, raw);
  }

  s.set_scale(factor);
  double test = s.get_units(CAL_AVG);
  Serial.printf("[%s] Test reading with weight on: %.2f g (expect ~%.1f)\n", name, test, KNOWN_WEIGHT_G);

  if (test < 0) {
    s.set_scale(-factor);
    test = s.get_units(CAL_AVG);
    factor = -factor;
    Serial.printf("[%s] Flipped sign. Test now: %.2f g\n", name, test);
  }

  Serial.printf("[%s] Remove the reference weight...\n", name);
  delay(1500);
  double zeroCheck = s.get_units(CAL_AVG);
  Serial.printf("[%s] Zero check (should be ~0): %.2f g\n", name, zeroCheck);

  return factor;
}

void saveCalibration(double foodF, double waterF) {
  prefs.begin("woof-cal", false);
  prefs.putDouble("food", foodF);
  prefs.putDouble("water", waterF);
  prefs.end();
}

bool loadCalibration(double& foodF, double& waterF) {
  prefs.begin("woof-cal", true);
  bool ok = prefs.isKey("food") && prefs.isKey("water");
  if (ok) {
    foodF  = prefs.getDouble("food", 0.0);
    waterF = prefs.getDouble("water", 0.0);
  }
  prefs.end();
  return ok;
}

void sendToFirebase(const String& url, const String& json) {
  if (WiFi.status() != WL_CONNECTED) return;
  secure.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(secure, url)) {
    Serial.println("HTTP begin() failed");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(json);
  Serial.printf("HTTP code: %d\n", code);
  if (code <= 0) {
    Serial.printf("HTTP error: %s\n", http.errorToString(code).c_str());
  } else {
    // Optional: Serial.println(http.getString());
  }
  http.end();
}

// ------------- Setup -------------

void setup() {
  Serial.begin(115200);
  delay(200);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WIFI");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println("\nConnected");

  // HX711 init
  foodScale.begin(FOOD_DOUT, FOOD_SCK);
  waterScale.begin(WATER_DOUT, WATER_SCK);

  if (loadCalibration(foodCal, waterCal) && foodCal != 0.0 && waterCal != 0.0) {
    foodScale.set_scale(foodCal);
    waterScale.set_scale(waterCal);
    foodScale.tare(CAL_AVG);
    waterScale.tare(CAL_AVG);
    calibrated = true;
    Serial.printf("Loaded calibration: FOOD=%.3f, WATER=%.3f\n", foodCal, waterCal);
  } else {
    Serial.println("=== Calibration ===");
    Serial.println("Make sure EMPTY BOWLS are on the sensors before continuing!");
    delay(3000);

    foodCal  = calibrateOne(foodScale,  "FOOD");
    waterCal = calibrateOne(waterScale, "WATER");
    saveCalibration(foodCal, waterCal);
    calibrated = true;

    Serial.printf("Saved calibration: FOOD=%.3f, WATER=%.3f\n", foodCal, waterCal);
    Serial.println("Starting loop...");
    delay(1500);
  }
}

// -------------- Loop --------------
void loop() {
  if (!calibrated) return;

  if (!waitReady(foodScale) || !waitReady(waterScale)) {
    Serial.println("Scales not ready, skip cycle");
    delay(300);
    return;
  }

  double foodG  = foodScale.get_units(LOOP_AVG);
  double waterG = waterScale.get_units(LOOP_AVG);

  if (foodG  < 0) foodG  = 0;
  if (waterG < 0) waterG = 0;

  Serial.printf("Food: %.2f g | Water: %.2f g\n", foodG, waterG);

  String json = "{";
  json += "\"food\": {\"weight\": " + String(foodG, 2) + "},";
  json += "\"water\": {\"weight\": " + String(waterG, 2) + "}";
  json += "}";

  sendToFirebase(FIREBASE_URL, json);

  delay(5000);
}
