// CONTROLROOM TLS-enabled sketch (abridged)
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>
#include <Servo.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define SS_PIN 11
#define RST_PIN 4
#define SERVO_PIN 7
#define IR_SENSOR_PIN 0
#define PIR_PIN 2
#define VIBRATION_PIN A2
#define BUZZER_PIN 6

char ssid[] = "Fairy";
char pass[] = "1608z246<3";
const char EDGE_HOST[] = "edge.example.com";
const uint16_t EDGE_PORT = 443;
const char EDGE_API_KEY[] = "REPLACE_WITH_EDGE_API_KEY";

MFRC522 rfid(SS_PIN, RST_PIN);
Servo doorServo;
WiFiUDP ntpUDP; NTPClient timeClient(ntpUDP, "pool.ntp.org");

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) { delay(200); }
}

bool sendToEdgeTLS(String jsonPayload) {
  ensureWiFi();
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiSSLClient client;
  if (!client.connect(EDGE_HOST, EDGE_PORT)) return false;
  String req = "POST /ingest HTTP/1.1\r\n";
  req += "Host: " + String(EDGE_HOST) + "\r\n";
  req += "Authorization: Bearer " + String(EDGE_API_KEY) + "\r\n";
  req += "Content-Type: application/json\r\n";
  req += "Content-Length: " + String(jsonPayload.length()) + "\r\n";
  req += "Connection: close\r\n\r\n";
  req += jsonPayload;
  client.print(req);
  unsigned long start = millis();
  while (!client.available() && millis() - start < 2000) delay(10);
  String resp="";
  while (client.available()) resp += (char)client.read();
  client.stop();
  return true;
}
