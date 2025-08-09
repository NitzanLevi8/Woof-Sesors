קוד עובד לGPS WOOF

#define MODEM Serial1

const String SENSOR_ID = "GPSsensor1";
const String FIREBASE_URL = "https://woof-gps-default-rtdb.europe-west1.firebasedatabase.app/location/" + SENSOR_ID + ".json";

// ========== AT Command=========
void sendAT(String command, int wait = 1000) {
  SerialUSB.println(">>> " + command);
  MODEM.println(command);
  delay(wait);
  while (MODEM.available()) {
    SerialUSB.write(MODEM.read());
  }
  SerialUSB.println("-----------------------");
}

// ======= Initializing  ==========
void setup() {
  SerialUSB.begin(115200);
  while (!SerialUSB);
  MODEM.begin(115200);
  delay(3000);

  SerialUSB.println("Starting GPS...");
  MODEM.println("AT+CGPS=1,1");  // הפעלת GPS
  delay(2000);
}

// ========== Loop ==========
void loop() {
  SerialUSB.println("Checking Location...");
  MODEM.println("AT+CGPSINFO");
  delay(1000);

  String response = readGPSResponse();
  SerialUSB.println("GPS response");
  SerialUSB.println(response);

  String lat, lon;
  if (parseGPS(response, lat, lon)) {
    String timestamp = extractTimestamp(response);
    String json = "{\"lat\":\"" + lat + "\",\"lon\":\"" + lon + "\",\"timestamp\":\"" + timestamp + "\"}";
    postToFirebase(json);
  } else {
    SerialUSB.println("No valid location data");
  }

  delay(10000); // Every 10 sec
}

// ========Sending POST to Firebase=========
void postToFirebase(String json) {
  SerialUSB.println("Sending to Firebase");
  SerialUSB.println(json);

  sendAT("AT+HTTPTERM", 500);
  sendAT("AT+HTTPINIT", 1000);
  sendAT("AT+HTTPPARA=\"CID\",1", 1000);
  sendAT("AT+HTTPPARA=\"URL\",\"" + FIREBASE_URL + "\"", 1000);
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);

  MODEM.println("AT+HTTPDATA=" + String(json.length()) + ",10000");
  delay(100);
  if (MODEM.find("DOWNLOAD")) {
    MODEM.print(json);
    delay(500);
  }

  sendAT("AT+HTTPACTION=1", 6000);  // POST
  sendAT("AT+HTTPTERM", 500);
}

// ======= GPS respons =========
String readGPSResponse() {
  String res = "";
  unsigned long timeout = millis() + 3000;
  while (millis() < timeout) {
    while (MODEM.available()) {
      char c = MODEM.read();
      res += c;
    }
  }
  return res;
}

// ========Converting degree to decimal==========
float convertDMMtoDecimal(float dmm, String direction) {
  int degrees = int(dmm / 100);
  float minutes = dmm - (degrees * 100);
  float decimal = degrees + (minutes / 60.0);
  if (direction == "S" || direction == "W") decimal *= -1;
  return decimal;
}

// ==========parsing GPS respons==========
bool parseGPS(String response, String &latOut, String &lonOut) {
  int start = response.indexOf(":");
  if (start == -1) return false;

  String data = response.substring(start + 1);
  data.trim();

  String fields[10];
  int fieldIndex = 0, fromIndex = 0;

  for (int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == ',' || i == data.length() - 1) {
      fields[fieldIndex++] = data.substring(fromIndex, (i == data.length() - 1) ? i + 1 : i);
      fromIndex = i + 1;
      if (fieldIndex >= 10) break;
    }
  }

  if (fields[0].length() < 4 || fields[2].length() < 4) return false;

  latOut = String(convertDMMtoDecimal(fields[0].toFloat(), fields[1]), 6);
  lonOut = String(convertDMMtoDecimal(fields[2].toFloat(), fields[3]), 6);
  return true;
}

// ========timestamp from GPS =========
String extractTimestamp(String response) {
  int start = response.indexOf(":");
  if (start == -1) return "";

  String data = response.substring(start + 1);
  data.trim();

  String fields[10];
  int fieldIndex = 0, fromIndex = 0;

  for (int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == ',' || i == data.length() - 1) {
      fields[fieldIndex++] = data.substring(fromIndex, (i == data.length() - 1) ? i + 1 : i);
      fromIndex = i + 1;
      if (fieldIndex >= 10) break;
    }
  }

  if (fields[4].length() != 6 || fields[5].length() < 6) return "";

  String yy = fields[4].substring(0, 2);
  String mm = fields[4].substring(2, 4);
  String dd = fields[4].substring(4, 6);
  String date = "20" + yy + "-" + mm + "-" + dd;

  String hh = fields[5].substring(0, 2);
  String mi = fields[5].substring(2, 4);
  String ss = fields[5].substring(4, 6);
  String time = hh + ":" + mi + ":" + ss;

  return date + " " + time;
}
