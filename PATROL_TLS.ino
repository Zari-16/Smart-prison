/*
 PATROL_TLS.ino
 Patrol guard sketch adapted to send telemetry to Edge ingestion endpoint over HTTPS/TLS.
 - Reads DHT22, MQ-135, water sensor, PIR.
 - Sends JSON to Edge /ingest using WiFiSSLClient with Bearer token.
 - Edge will handle ML inference and write to InfluxDB & MQTT alerts.
*/

#include <WiFiNINA.h>
#include "DHT.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTPIN A2
#define DHTTYPE DHT22

#define MQ135_PIN A1
#define WATER_PIN A0
#define PIR_PIN 7
#define BUZZER_PIN 6

DHT dht(DHTPIN, DHTTYPE);

// WiFi + NTP
char ssid[] = "Fairy";
char pass[] = "1608z246<3";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// Edge TLS server
const char EDGE_HOST[] = "edge.example.com"; // change to your domain/IP
const uint16_t EDGE_PORT = 443;
const char EDGE_API_KEY[] = "REPLACE_WITH_EDGE_API_KEY";

const unsigned long LOOP_INTERVAL_MS = 5000UL; // 5 seconds
unsigned long lastLoop = 0;

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("[WiFi] Attempting reconnect...");
  long start = millis();
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED && (millis() - start < 15000)) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Reconnected.");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Reconnect timed out.");
  }
}

bool sendToEdgeTLS(String payload) {
  ensureWiFi();
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[Edge] No WiFi"); return false; }
  WiFiSSLClient client;
  Serial.print("[Edge] TLS connecting to "); Serial.print(EDGE_HOST); Serial.print(":"); Serial.println(EDGE_PORT);
  if (!client.connect(EDGE_HOST, EDGE_PORT)) {
    Serial.println("[Edge] TLS connect failed");
    return false;
  }
  String req = String("POST /ingest HTTP/1.1\r\n");
  req += "Host: " + String(EDGE_HOST) + ":" + String(EDGE_PORT) + "\r\n";
  req += "Authorization: Bearer " + String(EDGE_API_KEY) + "\r\n";
  req += "Content-Type: application/json\r\n";
  req += "Content-Length: " + String(payload.length()) + "\r\n";
  req += "Connection: close\r\n\r\n";
  req += payload;
  client.print(req);
  unsigned long start = millis();
  while (!client.available() && millis() - start < 2000) delay(10);
  String resp = "";
  while (client.available()) resp += (char)client.read();
  Serial.print("[Edge] Resp: "); Serial.println(resp);
  client.stop();
  return true;
}

void debugPrintHeader() {
  Serial.println(F("========================================"));
  Serial.print(F("Timestamp (ms): ")); Serial.println(millis());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  delay(2000);
  Serial.println("Patrol Unit (Edge TLS) starting...");
  dht.begin();
  pinMode(MQ135_PIN, INPUT);
  pinMode(WATER_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("\nWiFi connected");
  else Serial.println("\nWiFi failed - will retry in loop");

  timeClient.begin();
  timeClient.update();
  lastLoop = millis();
}

void loop() {
  if (millis() - lastLoop < LOOP_INTERVAL_MS) return;
  lastLoop = millis();
  debugPrintHeader();

  ensureWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Main] Skipped data read/send due to Wi-Fi disconnection.");
    return;
  }

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int gasValue = analogRead(MQ135_PIN);
  int waterValue = analogRead(WATER_PIN);
  int pirValue = digitalRead(PIR_PIN);

  Serial.println(F("---- SENSOR READINGS ----"));
  if (isnan(temperature)) Serial.println(F("Temp: ERROR (NaN)")); else { Serial.print(F("Temp: ")); Serial.print(temperature, 2); Serial.println(F(" °C")); }
  if (isnan(humidity)) Serial.println(F("Humidity: ERROR (NaN)")); else { Serial.print(F("Humidity: ")); Serial.print(humidity, 2); Serial.println(F(" %")); }
  Serial.print(F("MQ-135 (raw): ")); Serial.println(gasValue);
  Serial.print(F("Water sensor (raw): ")); Serial.println(waterValue);
  Serial.print(F("PIR state (digital): ")); Serial.println(pirValue);
  Serial.println(F("-------------------------"));

  bool alarmState = false;
  if (gasValue > 500) { Serial.println(F("[ALARM] High gas")); alarmState = true; }
  if (waterValue > 300) { Serial.println(F("[ALARM] Water detected")); alarmState = true; }

  if (alarmState) digitalWrite(BUZZER_PIN, HIGH);
  else digitalWrite(BUZZER_PIN, LOW);

  // Build JSON payload
  timeClient.update();
  String payload = "{\"device\":\"patrol_unit\",\"ts\":" + String(timeClient.getEpochTime());
  payload += ",\"temp\":" + String(temperature,2);
  payload += ",\"humidity\":" + String(humidity,2);
  payload += ",\"gas\":" + String(gasValue);
  payload += ",\"water\":" + String(waterValue);
  payload += ",\"pir\":" + String(pirValue);
  payload += "}";
  bool ok = sendToEdgeTLS(payload);
  if (ok) Serial.println(F("[Main] Sent data to Edge."));
  else Serial.println(F("[Main] Edge send failed."));
  Serial.println(F("Loop cycle complete."));
}
