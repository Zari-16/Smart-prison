// PATROL TLS-enabled sketch (abridged)
#include <WiFiNINA.h>
#include "DHT.h"
#include <WiFiUdp.h>
#include <NTPClient.h>

#define DHTPIN A2
#define DHTTYPE DHT22
#define MQ135_PIN A1
#define WATER_PIN A0
#define PIR_PIN 7

char ssid[] = "Fairy";
char pass[] = "1608z246<3";
const char EDGE_HOST[] = "edge.example.com";
const uint16_t EDGE_PORT = 443;
const char EDGE_API_KEY[] = "REPLACE_WITH_EDGE_API_KEY";

DHT dht(DHTPIN, DHTTYPE);
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

void setup(){
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  timeClient.begin();
  dht.begin();
}

void loop() {
  timeClient.update();
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int gas = analogRead(MQ135_PIN);
  int water = analogRead(WATER_PIN);
  int pir = digitalRead(PIR_PIN);
  String j = "{\"device\":\"patrol_unit\",\"ts\":" + String(timeClient.getEpochTime()) + ",\"temp\":" + String(temp,2) + ",\"humidity\":" + String(hum,2) + ",\"gas\":" + String(gas) + ",\"water\":" + String(water) + ",\"pir\":" + String(pir) + "}";
  sendTelemetryTLS(j);
  delay(5000);
}
