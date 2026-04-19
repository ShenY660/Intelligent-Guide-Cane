#include "network.h"

namespace {

String readHttpResponseBody(HTTPClient &http)
{
  String payload = http.getString();
  if (payload.length() > 0)
  {
    return payload;
  }

  WiFiClient *stream = http.getStreamPtr();
  if (stream == nullptr)
  {
    return "";
  }

  String streamedPayload;
  int expectedLength = http.getSize();
  if (expectedLength > 0)
  {
    streamedPayload.reserve(expectedLength);
  }

  unsigned long lastReadAt = millis();
  while (http.connected() && (millis() - lastReadAt) < 1500)
  {
    while (stream->available())
    {
      streamedPayload += static_cast<char>(stream->read());
      lastReadAt = millis();
    }
    delay(10);
  }

  return streamedPayload;
}

String buildHttpErrorJson(int httpResponseCode, const String &detail)
{
  DynamicJsonDocument jsonDocument(256);
  jsonDocument["error"] = detail;
  jsonDocument["http_status"] = httpResponseCode;

  String jsonStr;
  serializeJson(jsonDocument, jsonStr);
  return jsonStr;
}

} // namespace

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
  jsonDocument["device_id"] = DEVICE_ID;
  String jsonStr;
  serializeJson(jsonDocument, jsonStr);

  HTTPClient http;
  const char *headerKeys[] = {"Content-Length", "Transfer-Encoding"};
  http.useHTTP10(true);
  http.setReuse(false);
  http.setTimeout(10000);
  http.collectHeaders(headerKeys, 2);
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  Serial.printf("[AI] POST %s | device=%s | payload=%s\n",
                serverUrl.c_str(), DEVICE_ID, jsonStr.c_str());
  int httpResponseCode = http.POST(jsonStr);

  if (httpResponseCode <= 0)
  {
    String errorDetail = String("HTTP request failed: ") + http.errorToString(httpResponseCode);
    Serial.printf("[AI] %s\n", errorDetail.c_str());
    http.end();
    return buildHttpErrorJson(httpResponseCode, errorDetail);
  }

  String payload = readHttpResponseBody(http);
  String contentLength = http.header("Content-Length");
  String transferEncoding = http.header("Transfer-Encoding");
  Serial.printf("[AI] HTTP=%d | len=%s | transfer=%s | body_len=%u\n",
                httpResponseCode,
                contentLength.length() > 0 ? contentLength.c_str() : "(none)",
                transferEncoding.length() > 0 ? transferEncoding.c_str() : "(none)",
                payload.length());

  if (payload.length() > 0)
  {
    Serial.printf("[AI] body=%s\n", payload.c_str());
    http.end();
    return payload;
  }

  String emptyBodyDetail = String("Empty response body from server");
  Serial.printf("[AI] %s\n", emptyBodyDetail.c_str());
  http.end();
  return buildHttpErrorJson(httpResponseCode, emptyBodyDetail);
}

bool sendGpsData(double latitude, double longitude)
{
  String serverUrl = String(SERVER_BASE_URL) + "/gps";

  DynamicJsonDocument jsonDocument(256);
  jsonDocument["latitude"] = latitude;
  jsonDocument["longitude"] = longitude;
  jsonDocument["device_id"] = DEVICE_ID;
  String jsonStr;
  serializeJson(jsonDocument, jsonStr);

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  Serial.printf("[GPS] POST %s | device=%s | payload={\"latitude\":%.6f,\"longitude\":%.6f}\n",
                serverUrl.c_str(), DEVICE_ID, latitude, longitude);
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
