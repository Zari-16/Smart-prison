/*
 CONTROLROOM12_TLS.ino
 Full control-room sketch adapted to send telemetry and events to Edge ingestion endpoint over HTTPS/TLS.
 - Uses WiFiNINA WiFiSSLClient for TLS connections to Edge (nginx / uvicorn).
 - Use EDGE_HOST and EDGE_PORT (443) pointing to your edge domain (nginx reverse proxy recommended).
 - Set EDGE_API_KEY to the same value as edge/config.yaml server.api_key.
 - Removes direct Influx writes from Arduino; Edge handles DB writes and ML inference.
*/

#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SS_PIN 11
#define RST_PIN 4
#define SERVO_PIN 7
#define IR_SENSOR_PIN 0
#define PIR_PIN 2
#define VIBRATION_PIN A2
#define BUZZER_PIN 6

MFRC522 rfid(SS_PIN, RST_PIN);
Servo doorServo;

char ssid[] = "Fairy";
char pass[] = "1608z246<3";

// Edge server (set to your domain or IP where nginx/uvicorn listens)
const char EDGE_HOST[] = "edge.example.com";  // replace with your domain/IP
const uint16_t EDGE_PORT = 443;
const char EDGE_API_KEY[] = "REPLACE_WITH_EDGE_API_KEY";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// Timing
const unsigned long INFLUX_INTERVAL_MS = 5000UL;    // telemetry interval (sent to Edge)
unsigned long lastInfluxUpdate = 0;

const unsigned long SENSOR_CHECK_MS = 300UL;        // Check sensors frequently
unsigned long lastSensorCheck = 0;

volatile bool irTriggered = false;
volatile unsigned long lastIRTime = 0;
const unsigned long irDebounce = 500;

int peopleCount = 0;
bool doorOpen = false;
unsigned long doorOpenTime = 0;
bool countingActive = false;
String lastGuardName = "Nobody";

String currentDoorMessage = "CLOSED";
unsigned long deniedMessageUntil = 0;
const unsigned long DENIED_DISPLAY_TIME = 5000;

int vibThreshold = 300;
bool fenceAlertActive = false;
unsigned long lastFenceTrigger = 0;
const unsigned long fenceCooldown = 10000;

// Authorized cards (keep updated securely)
String validUID_Level2[] = {"13B7750F", "ABCD1234"};  // Senior guards → can open
String validUID_Level1[] = {"233400F8", "93F84E2A"};  // Normal guards → denied

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("WiFi disconnected, reconnecting...");
  WiFi.disconnect();
  delay(250);
  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected!");
  } else {
    Serial.println("\nWiFi connection failed - will retry in background");
  }
}

void irISR() {
  unsigned long now = millis();
  if (now - lastIRTime > irDebounce) {
    lastIRTime = now;
    irTriggered = true;
  }
}

void buzzerLevel2Granted() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, LOW); delay(150);
    digitalWrite(BUZZER_PIN, HIGH); delay(150);
  }
  digitalWrite(BUZZER_PIN, HIGH);
}

void buzzerLevel1Denied() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(BUZZER_PIN, LOW); delay(100);
    digitalWrite(BUZZER_PIN, HIGH); delay(100);
  }
  digitalWrite(BUZZER_PIN, HIGH);
}

void buzzerUnknownCard() {
  digitalWrite(BUZZER_PIN, LOW); delay(3000); digitalWrite(BUZZER_PIN, HIGH);
}

void openDoor() {
  if (doorOpen) return;
  Serial.println("DOOR OPENING");
  doorServo.write(90);
  doorOpen = true;
  doorOpenTime = millis();
  peopleCount = 0;
  countingActive = true;
}

void manageDoor() {
  if (doorOpen && (millis() - doorOpenTime >= 5000)) {
    Serial.println("DOOR CLOSING");
    doorServo.write(0);
    doorOpen = false;
    countingActive = false;
    currentDoorMessage = "CLOSED";
    peopleCount = 0;
  }
}

// Send JSON over HTTPS to Edge /ingest using WiFiSSLClient
bool sendToEdgeTLS(const String &jsonPayload) {
  ensureWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Edge] No WiFi");
    return false;
  }
  WiFiSSLClient client;
  Serial.print("[Edge] Connecting to "); Serial.print(EDGE_HOST); Serial.print(":"); Serial.println(EDGE_PORT);
  if (!client.connect(EDGE_HOST, EDGE_PORT)) {
    Serial.println("[Edge] TLS connect failed");
    return false;
  }
  String req = String("POST /ingest HTTP/1.1\r\n");
  req += String("Host: ") + String(EDGE_HOST) + "\r\n";
  req += String("Authorization: Bearer ") + String(EDGE_API_KEY) + "\r\n";
  req += "Content-Type: application/json\r\n";
  req += "Content-Length: " + String(jsonPayload.length()) + "\r\n";
  req += "Connection: close\r\n\r\n";
  req += jsonPayload;
  client.print(req);

  unsigned long start = millis();
  while (!client.available() && millis() - start < 2000) {
    delay(10);
  }
  String resp = "";
  while (client.available()) {
    resp += (char)client.read();
  }
  Serial.print("[Edge] Resp: "); Serial.println(resp);
  client.stop();
  return true;
}

void testEdgeConnection() {
  Serial.println("=== TEST EDGE CONNECT ===");
  String test = "{\"device\":\"control_room_test\",\"ts\":" + String(timeClient.getEpochTime()) + ",\"test\":1}";
  sendToEdgeTLS(test);
}

// =================== RFID (adapted) ===================
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += (rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println("RFID Detected: " + uid);

  String name = "Unknown";
  int level = 0;

  for (String id : validUID_Level2) {
    if (uid.indexOf(id) != -1) {
      level = 2;
      name = "Senior:" + uid;
      lastGuardName = name;
      Serial.println("LEVEL 2 AUTHORIZED"); break;
    }
  }
  if (level == 0) {
    for (String id : validUID_Level1) {
      if (uid.indexOf(id) != -1) {
        level = 1;
        name = "Guard:" + uid;
        lastGuardName = name;
        Serial.println("LEVEL 1 DETECTED"); break;
      }
    }
  }

  if (level == 2) {
    currentDoorMessage = "OPENED by " + name;
    deniedMessageUntil = 0;
    openDoor();
    buzzerLevel2Granted();
    // send event to Edge
    String js = "{\"device\":\"control_room\",\"ts\":" + String(timeClient.getEpochTime()) + ",\"event\":\"rfid_open\",\"uid\":\"" + uid + "\",\"level\":2}";
    sendToEdgeTLS(js);
  } else if (level == 1) {
    Serial.println("ACCESS DENIED - LEVEL 1");
    currentDoorMessage = "LEVEL 1 DENIED";
    deniedMessageUntil = millis() + DENIED_DISPLAY_TIME;
    buzzerLevel1Denied();
    String js = "{\"device\":\"control_room\",\"ts\":" + String(timeClient.getEpochTime()) + ",\"event\":\"rfid_denied\",\"uid\":\"" + uid + "\",\"level\":1}";
    sendToEdgeTLS(js);
  } else {
    Serial.println("ACCESS DENIED - UNKNOWN CARD");
    currentDoorMessage = "ACCESS DENIED";
    deniedMessageUntil = millis() + DENIED_DISPLAY_TIME;
    buzzerUnknownCard();
    String js = "{\"device\":\"control_room\",\"ts\":" + String(timeClient.getEpochTime()) + ",\"event\":\"rfid_unknown\",\"uid\":\"" + uid + "\"}";
    sendToEdgeTLS(js);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}

// People counting
void handleIRCounting() {
  if (irTriggered && countingActive && doorOpen) {
    irTriggered = false;
    peopleCount++;
    Serial.println("Person counted: " + String(peopleCount));
  }
}

// Fence/perimeter sensors
void checkFenceSensors() {
  int pirVal = digitalRead(PIR_PIN);
  int vibVal = analogRead(VIBRATION_PIN);

  if (pirVal == HIGH && vibVal > vibThreshold) {
    if (!fenceAlertActive && (millis() - lastFenceTrigger > fenceCooldown)) {
      fenceAlertActive = true;
      lastFenceTrigger = millis();
      Serial.println("FENCE ALERT!");
      String js = "{\"device\":\"control_room\",\"ts\":" + String(timeClient.getEpochTime()) + ",\"event\":\"fence_alert\",\"vib\":" + String(vibVal) + ",\"pir\":" + String(pirVal) + "}";
      sendToEdgeTLS(js);
    }
  } else if (fenceAlertActive && (millis() - lastFenceTrigger > 5000)) {
    fenceAlertActive = false;
    Serial.println("Fence alert cleared");
  }
}

// Telemetry send aggregator
void sendTelemetryToEdge() {
  if (deniedMessageUntil != 0 && millis() >= deniedMessageUntil) {
    if (!doorOpen) currentDoorMessage = "CLOSED";
    deniedMessageUntil = 0;
  }
  int doorState = 0;
  if (currentDoorMessage.indexOf("OPENED") != -1) doorState = 1;
  else if (currentDoorMessage.indexOf("DENIED") != -1) doorState = -1;
  int pirVal = digitalRead(PIR_PIN);
  int vibVal = analogRead(VIBRATION_PIN);

  String json = "{\"device\":\"control_room\",\"ts\":" + String(timeClient.getEpochTime());
  json += ",\"people\":" + String(peopleCount);
  json += ",\"door_open\":" + String(doorOpen ? 1 : 0);
  json += ",\"door_state\":" + String(doorState);
  json += ",\"fence_alert\":" + String(fenceAlertActive ? 1 : 0);
  json += ",\"pir\":" + String(pirVal);
  json += ",\"vib\":" + String(vibVal);
  json += ",\"vib_threshold\":" + String(vibThreshold);
  json += "}";
  sendToEdgeTLS(json);
}

void logToSerial() {
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 5000) {
    lastLog = millis();
    Serial.println("\n=== SENSOR DATA ===");
    Serial.println("Timestamp: " + String(millis() / 1000) + "s");
    Serial.println("People Count: " + String(peopleCount));
    Serial.println("Door: " + String(doorOpen ? "OPEN" : "CLOSED"));
    Serial.println("Fence Alert: " + String(fenceAlertActive ? "ACTIVE" : "OK"));
    Serial.println("PIR: " + String(digitalRead(PIR_PIN)));
    Serial.println("Vibration: " + String(analogRead(VIBRATION_PIN)));
    Serial.println("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
    Serial.println("===================\n");
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("\n=== PRISON RFID SYSTEM STARTING (Edge TLS) ===");

  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID initialized");

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);
  Serial.println("Door servo initialized");

  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_SENSOR_PIN), irISR, FALLING);
  Serial.println("IR sensor initialized");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

  pinMode(PIR_PIN, INPUT);
  pinMode(VIBRATION_PIN, INPUT);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
  } else {
    Serial.println("\nWiFi connection failed - will retry in background");
  }

  timeClient.begin();
  timeClient.update();

  testEdgeConnection();

  lastInfluxUpdate = millis();
  lastSensorCheck = millis();

  Serial.println("=== SYSTEM READY ===\n");
}

void loop() {
  // Critical: check RFID frequently (no blocking)
  checkRFID();
  manageDoor();

  // frequent sensor checks
  if (millis() - lastSensorCheck >= SENSOR_CHECK_MS) {
    lastSensorCheck = millis();
    handleIRCounting();
    checkFenceSensors();
  }

  // send telemetry to Edge every 5s
  if (millis() - lastInfluxUpdate >= INFLUX_INTERVAL_MS) {
    lastInfluxUpdate = millis();
    sendTelemetryToEdge();
  }

  logToSerial();
  delay(10);
}
