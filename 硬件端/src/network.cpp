#include "network.h"

void initWiFi()
{
  Serial.println("Starting WiFi connection...");
  Serial.printf("  SSID: %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  IPAddress noIp(0, 0, 0, 0);
  IPAddress primaryDns(223, 5, 5, 5);
  IPAddress secondaryDns(8, 8, 8, 8);
  bool dnsConfigured = WiFi.config(noIp, noIp, noIp, primaryDns, secondaryDns);
  Serial.printf("  DNS config: %s | primary=%s secondary=%s\n",
                dnsConfigured ? "OK" : "FAIL",
                primaryDns.toString().c_str(),
                secondaryDns.toString().c_str());

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  const int maxAttempts = 20;

  Serial.print("  Connecting");
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
  {
    delay(1000);
    Serial.print(".");
    attempts++;

    if (attempts % 5 == 0)
    {
      Serial.printf(" [%d s, status %d] ", attempts, WiFi.status());
    }
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi connected");
    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("  MAC: %s\n", WiFi.macAddress().c_str());
    Serial.printf("  DNS1: %s\n", WiFi.dnsIP(0).toString().c_str());
    Serial.printf("  DNS2: %s\n", WiFi.dnsIP(1).toString().c_str());
    isConnectedToWifi = true;
  }
  else
  {
    Serial.println("WiFi connection failed");
    Serial.printf("  Final status: %d\n", WiFi.status());
    isConnectedToWifi = false;
  }
}

String sendTextToServer(String text)
{
  String serverUrl = String(SERVER_BASE_URL) + "/ai";

  DynamicJsonDocument jsonDocument(1024);
  jsonDocument["message"] = text;
  String jsonStr;
  serializeJson(jsonDocument, jsonStr);

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(jsonStr);
  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    if (payload.length() > 0)
    {
      http.end();
      return payload;
    }
  }
  http.end();
  return "Request failed";
}

bool sendGpsData(double latitude, double longitude)
{
  String serverUrl = String(SERVER_BASE_URL) + "/gps";

  DynamicJsonDocument jsonDocument(256);
  jsonDocument["latitude"] = latitude;
  jsonDocument["longitude"] = longitude;
  String jsonStr;
  serializeJson(jsonDocument, jsonStr);

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  Serial.printf("[GPS] POST %s | payload={\"latitude\":%.6f,\"longitude\":%.6f}\n",
                serverUrl.c_str(), latitude, longitude);
  int httpResponseCode = http.POST(jsonStr);

  bool ok = httpResponseCode >= 200 && httpResponseCode < 300;
  String payload = http.getString();
  if (!ok)
  {
    Serial.printf("[GPS] Server rejected upload | http=%d body=%s\n",
                  httpResponseCode, payload.c_str());
  }
  else
  {
    Serial.printf("[GPS] Server acknowledged upload | http=%d body=%s\n",
                  httpResponseCode, payload.c_str());
  }

  http.end();
  return ok;
}
