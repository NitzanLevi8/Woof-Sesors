#define MODEM Serial1 // UART port connected to SIM7600E

// ========== Firebase target URL for storing GPS data ==========
const String SENSOR_ID = "GPSsensor1";
const String FIREBASE_URL = "https://woof-gps-default-rtdb.europe-west1.firebasedatabase.app/location/" + SENSOR_ID + ".json";

// ========== AT Command ==========================================
// sendAT: Sends an AT command to the modem via UART, waits for response, 
// and prints both the command and the modem's reply to the USB console.
// ================================================================
void sendAT(String command, int wait = 1000) {
  // Logs go to SerialUSB (USB console)
  SerialUSB.println(">>> " + command);
  // Send the command to the modem (Serial1)
  MODEM.println(command);
  // Wait and dump modem response to USB
  unsigned long endAt = millis() + wait;
  while (millis() < endAt) {
    while (MODEM.available()) {
      SerialUSB.write(MODEM.read());
    }
  }
  SerialUSB.println("-----------------------");
}

// ======= Initializing ===========================================
// setup: Runs once at startup. Opens USB log, starts UART with modem, 
// and sends AT command to turn on GPS.
// ================================================================

void setup() {
  // USB console
  SerialUSB.begin(115200);
  while (!SerialUSB) { /* wait for native USB */ }
  delay(300);
  SerialUSB.println("\n[BOOT] Maduino Zero + SIM7600E");

  // UART to modem
  MODEM.begin(115200);
  delay(2000);

  SerialUSB.println("Starting GPS...");
  MODEM.println("AT+CGPS=1,1");  // enable GPS standalone
  delay(2000);
}

// ========== Loop ================================================
// loop: Runs continuously. Requests GPS data, parses response, 
// sends valid coordinates + timestamp to Firebase every 10 seconds.
// ================================================================

void loop() {
  SerialUSB.println("Checking Location...");
  MODEM.println("AT+CGPSINFO");
  delay(1000);

  String response = readGPSResponse();
  SerialUSB.println("GPS response:");
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

// ======== Sending POST to Firebase ==============================
// postToFirebase: Prepares the modem's HTTP stack, sends the given JSON payload
// to the Firebase URL using HTTP POST, and terminates the HTTP session.
// Steps: HTTPTERM >> HTTPINIT >> set URL/CONTENT >> HTTPDATA (send JSON) >> HTTPACTION=1 >> HTTPTERM.
// ================================================================

void postToFirebase(String json) {
  SerialUSB.println("Sending to Firebase");
  SerialUSB.println(json);

 // Terminate any previous HTTP session
  sendAT("AT+HTTPTERM", 500);
  // Initialize HTTP service
  sendAT("AT+HTTPINIT", 1000);
  // Set connection profile ID (usually PDP context ID = 1)
  sendAT("AT+HTTPPARA=\"CID\",1", 1000);
  // Set the target URL (Firebase endpoint)
  sendAT("AT+HTTPPARA=\"URL\",\"" + FIREBASE_URL + "\"", 1000);
  // Set content type header to JSON
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);

  // Tell modem that we are about to send JSON data (length, timeout)
  MODEM.println("AT+HTTPDATA=" + String(json.length()) + ",10000");
  // Wait up to 3 seconds for "DOWNLOAD" prompt from modem
  unsigned long endAt = millis() + 3000;
  bool gotDownload = false;
  while (millis() < endAt) {
    // Look for the "DOWNLOAD" keyword in modem response
    if (MODEM.find("DOWNLOAD")) { gotDownload = true; break; }
  }
  // If "DOWNLOAD" prompt received so send JSON body
  if (gotDownload) {
    MODEM.print(json);
    delay(500);
  } else {
    SerialUSB.println("HTTPDATA DOWNLOAD not received");
  }
  
  sendAT("AT+HTTPACTION=1", 6000);  // POST
  sendAT("AT+HTTPTERM", 500);   // Terminate HTTP service after request
}

// ======= GPS response ===========================================
// readGPSResponse: Reads all available characters from the modem
// and returns them as a complete response string.
// ================================================================
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

// ======== Converting degree to decimal ==========================
// Converts GPS coordinates from degrees and minutes format
// into standard decimal degrees. Adjusts sign for South/West directions.
// ================================================================

float convertDMMtoDecimal(float dmm, String direction) {
  int degrees = int(dmm / 100);
  float minutes = dmm - (degrees * 100);
  float decimal = degrees + (minutes / 60.0);
  if (direction == "S" || direction == "W") decimal *= -1;
  return decimal;
}

// ========== parsing GPS response ================================
// Parses the raw GPS response string and extracts latitude/longitude.
// Returns true if valid coordinates are found, false otherwise.
// ================================================================

bool parseGPS(String response, String &latOut, String &lonOut) {
  // Find the position of ':' in the response (start of data)
  int start = response.indexOf(":");
  if (start == -1) return false;

  // Extract substring after ':' (the actual GPS data)
  String data = response.substring(start + 1);
  data.trim();

  // Prepare array to hold up to 10 comma separated fields
  String fields[10];
  int fieldIndex = 0, fromIndex = 0;

   // Split the data string by commas into fields[]
  for (int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == ',' || i == data.length() - 1) {
      fields[fieldIndex++] = data.substring(fromIndex, (i == data.length() - 1) ? i + 1 : i);
      fromIndex = i + 1;
      if (fieldIndex >= 10) break;
    }
  }
  
  // Check that latitude (field[0]) and longitude (field[2]) have enough characters
  if (fields[0].length() < 4 || fields[2].length() < 4) return false;

  // Convert latitude and longitude from DMM to decimal format
  latOut = String(convertDMMtoDecimal(fields[0].toFloat(), fields[1]), 6);
  lonOut = String(convertDMMtoDecimal(fields[2].toFloat(), fields[3]), 6);
  return true;
}

// ======== timestamp from GPS ====================================
// extractTimestamp: Parses GPS response and builds a date-time string (YYYY-MM-DD HH:MM:SS).
// Returns empty string if timestamp fields are invalid.
// ================================================================

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

  // Validate that date (fields[4]) and time (fields[5]) exist
  if (fields[4].length() != 6 || fields[5].length() < 6) return "";

  // Parse date: ddmmyy >> YYYY-MM-DD
  String yy = fields[4].substring(0, 2);
  String mm = fields[4].substring(2, 4);
  String dd = fields[4].substring(4, 6);
  String date = "20" + yy + "-" + mm + "-" + dd;

  // Parse time: hhmmss >> HH:MM:SS
  String hh = fields[5].substring(0, 2);
  String mi = fields[5].substring(2, 4);
  String ss = fields[5].substring(4, 6);
  String time = hh + ":" + mi + ":" + ss;

  return date + " " + time;
}
