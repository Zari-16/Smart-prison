// CONTROLROOM TLS-enabled sketch (abridged)
// NOTE: MKR WiFiNINA TLS support is limited; this sketch shows pattern for WiFiSSLClient
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
const char EDGE_HOST[] = "edge.example.com"; // use domain behind nginx+Let'sEncrypt
const uint16_t EDGE_PORT = 443;
const char EDGE_API_KEY[] = "REPLACE_WITH_EDGE_API_KEY";

MFRC522 rfid(SS_PIN, RST_PIN);
Servo doorServo;
WiFiUDP ntpUDP; NTPClient timeClient(ntpUDP, "pool.ntp.org");

void sendTelemetryTLS(String json) {
  WiFiSSLClient client;
  if (!client.connect(EDGE_HOST, EDGE_PORT)) { Serial.println("TLS connect failed"); return; }
  String req = "POST /ingest HTTP/1.1\r\nHost: " + String(EDGE_HOST) + "\r\n";
  req += "Authorization: Bearer " + String(EDGE_API_KEY) + "\r\n";
  req += "Content-Type: application/json\r\n";
  req += "Content-Length: " + String(json.length()) + "\r\n";
  req += "Connection: close\r\n\r\n";
  req += json;
  client.print(req);
  unsigned long start = millis();
  while (!client.available() && millis() - start < 2000) delay(10);
  while (client.available()) Serial.write(client.read());
  client.stop();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) { delay(500); Serial.print("."); }
  timeClient.begin(); timeClient.update();
}
void loop() {
  timeClient.update();
  String j = "{\"device\":\"control_room\",\"ts\":" + String(timeClient.getEpochTime()) + ",\"vib\":" + String(analogRead(VIBRATION_PIN)) + "}";
  sendTelemetryTLS(j);
  delay(5000);
}
