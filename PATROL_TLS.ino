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

bool sendToEdgeTLS(String jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) { WiFi.begin(ssid, pass); delay(2000); }
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
