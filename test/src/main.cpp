#include <Arduino.h>
#include <string.h>

namespace
{
HardwareSerial gpsSerial(1);

constexpr int GPS_RX_PIN = 17;
constexpr int GPS_TX_PIN = -1;
constexpr uint32_t GPS_BAUD_CANDIDATES[] = {9600, 115200, 38400, 57600, 4800};
constexpr size_t GPS_RX_BUFFER_SIZE = 2048;
constexpr size_t NMEA_BUFFER_SIZE = 160;
constexpr uint32_t STATUS_INTERVAL_MS = 5000;
constexpr uint32_t BAUD_SWITCH_INTERVAL_MS = 4000;

char nmeaBuffer[NMEA_BUFFER_SIZE];
size_t nmeaIndex = 0;

unsigned long bootTimeMs = 0;
unsigned long lastStatusLogMs = 0;
unsigned long lastDataSeenMs = 0;
unsigned long lastByteSeenMs = 0;
unsigned long lastBaudSwitchMs = 0;

uint32_t rawByteCount = 0;
uint32_t rawLineCount = 0;
uint32_t rmcCount = 0;
uint32_t validFixCount = 0;
uint32_t invalidFixCount = 0;

size_t currentBaudIndex = 0;
bool baudLocked = false;

String latestUtcTime;
double latestLatitude = 0.0;
double latestLongitude = 0.0;
bool hasValidFix = false;
String latestAntennaStatus = "UNKNOWN";

double nmeaToDecimal(const char *value)
{
  double raw = atof(value);
  int degrees = static_cast<int>(raw / 100);
  double minutes = raw - static_cast<double>(degrees * 100);
  return degrees + minutes / 60.0;
}

void printBanner()
{
  Serial.println();
  Serial.println("========================================");
  Serial.println("ATGM332D GPS Raw Passthrough Test");
  Serial.println("Board: ESP32-S3");
  Serial.printf("UART1 RX=%d TX=%d\n", GPS_RX_PIN, GPS_TX_PIN);
  Serial.println("Purpose:");
  Serial.println("1. Print raw NMEA sentences");
  Serial.println("2. Detect ANTENNA OK / OPEN / SHORT");
  Serial.println("3. Detect RMC valid fix state");
  Serial.println("4. Auto-scan common baud rates");
  Serial.println("========================================");
  Serial.println();
}

void printWiringHint()
{
  Serial.println("[GPS TEST] Wiring checklist:");
  Serial.println("  Module VCC -> ESP32 3.3V/5V (according to your module board)");
  Serial.println("  Module GND -> ESP32 GND");
  Serial.println("  Module TXD -> ESP32 GPIO17");
  Serial.println("  Module RXD -> leave unconnected for this test");
  Serial.println("  Module PPS -> leave unconnected for this test");
  Serial.println("  Avoid GPIO43/44 for GPS on ESP32-S3 devkit, they may conflict with download/serial.");
  Serial.println();
}

uint32_t currentBaud()
{
  return GPS_BAUD_CANDIDATES[currentBaudIndex];
}

void startGpsUart(uint32_t baud)
{
  gpsSerial.end();
  delay(50);
  gpsSerial.begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.printf("[GPS TEST] UART1 started | RX=%d TX=%d baud=%lu\n", GPS_RX_PIN, GPS_TX_PIN, baud);
}

void updateFix(const char *utcTime, const char *latValue, const char *ns, const char *lonValue, const char *ew)
{
  double latitude = nmeaToDecimal(latValue);
  double longitude = nmeaToDecimal(lonValue);

  if (ns[0] == 'S')
  {
    latitude = -latitude;
  }
  if (ew[0] == 'W')
  {
    longitude = -longitude;
  }

  latestUtcTime = utcTime;
  latestLatitude = latitude;
  latestLongitude = longitude;
  hasValidFix = true;
  validFixCount++;

  Serial.printf(
      "[GPS FIX] VALID | UTC=%s | lat=%.6f | lon=%.6f\n",
      latestUtcTime.c_str(),
      latestLatitude,
      latestLongitude);
}

void parseRmc(char *sentence)
{
  char *fields[16] = {nullptr};
  int fieldCount = 0;

  char *context = nullptr;
  char *token = strtok_r(sentence, ",", &context);
  while (token != nullptr && fieldCount < 16)
  {
    fields[fieldCount++] = token;
    token = strtok_r(nullptr, ",", &context);
  }

  rmcCount++;
  if (fieldCount < 7)
  {
    Serial.println("[GPS RMC] Ignored because field count is too small.");
    return;
  }

  if (strcmp(fields[2], "A") == 0)
  {
    updateFix(fields[1], fields[3], fields[4], fields[5], fields[6]);
  }
  else
  {
    hasValidFix = false;
    invalidFixCount++;
    Serial.printf("[GPS FIX] INVALID | UTC=%s | status=%s\n", fields[1], fields[2]);
  }
}

void parseTextStatus(const char *sentence)
{
  if (strstr(sentence, "ANTENNA OK") != nullptr)
  {
    latestAntennaStatus = "OK";
    Serial.println("[GPS ANT] ANTENNA OK");
  }
  else if (strstr(sentence, "ANTENNA OPEN") != nullptr)
  {
    latestAntennaStatus = "OPEN";
    Serial.println("[GPS ANT] ANTENNA OPEN");
  }
  else if (strstr(sentence, "ANTENNA SHORT") != nullptr)
  {
    latestAntennaStatus = "SHORT";
    Serial.println("[GPS ANT] ANTENNA SHORT");
  }
}

void handleSentence(char *sentence)
{
  if (sentence[0] != '$')
  {
    return;
  }

  rawLineCount++;
  lastDataSeenMs = millis();
  baudLocked = true;

  Serial.printf("[GPS RAW] %s\n", sentence);

  if (strncmp(sentence, "$GPRMC", 6) == 0 ||
      strncmp(sentence, "$GNRMC", 6) == 0 ||
      strncmp(sentence, "$BDRMC", 6) == 0)
  {
    parseRmc(sentence);
    return;
  }

  if (strncmp(sentence, "$GPTXT", 6) == 0 || strncmp(sentence, "$GNTXT", 6) == 0)
  {
    parseTextStatus(sentence);
  }
}

void readGpsStream()
{
  while (gpsSerial.available() > 0)
  {
    char c = static_cast<char>(gpsSerial.read());
    rawByteCount++;
    lastByteSeenMs = millis();

    if (c == '\r')
    {
      continue;
    }

    if (c == '\n')
    {
      nmeaBuffer[nmeaIndex] = '\0';
      if (nmeaIndex > 0)
      {
        handleSentence(nmeaBuffer);
      }
      nmeaIndex = 0;
      continue;
    }

    if (nmeaIndex < NMEA_BUFFER_SIZE - 1)
    {
      nmeaBuffer[nmeaIndex++] = c;
    }
    else
    {
      Serial.println("[GPS TEST] NMEA buffer overflow, dropping current line.");
      nmeaIndex = 0;
    }
  }
}

void printPeriodicStatus()
{
  unsigned long now = millis();
  if (now - lastStatusLogMs < STATUS_INTERVAL_MS)
  {
    return;
  }
  lastStatusLogMs = now;

  Serial.println();
  Serial.println("--------------- GPS STATUS ---------------");
  Serial.printf("[GPS TEST] Uptime: %lu ms\n", now - bootTimeMs);
  Serial.printf("[GPS TEST] Current baud: %lu%s\n", currentBaud(), baudLocked ? " (locked)" : " (scanning)");
  Serial.printf("[GPS TEST] Raw bytes seen: %lu\n", rawByteCount);
  Serial.printf("[GPS TEST] Raw lines seen: %lu\n", rawLineCount);
  Serial.printf("[GPS TEST] RMC lines seen: %lu\n", rmcCount);
  Serial.printf("[GPS TEST] Valid fixes: %lu\n", validFixCount);
  Serial.printf("[GPS TEST] Invalid fixes: %lu\n", invalidFixCount);
  Serial.printf("[GPS TEST] Antenna status: %s\n", latestAntennaStatus.c_str());

  if (lastByteSeenMs == 0)
  {
    Serial.println("[GPS TEST] No UART data received yet.");
    Serial.println("[GPS TEST] Check power, GND, TXD->GPIO17, and whether the module is really sending NMEA.");
  }
  else if (lastDataSeenMs == 0)
  {
    Serial.printf("[GPS TEST] UART bytes are arriving. Last byte %lu ms ago.\n", now - lastByteSeenMs);
    Serial.println("[GPS TEST] But no valid NMEA sentence starting with '$' was assembled.");
    Serial.println("[GPS TEST] This strongly suggests a baud mismatch or line noise.");
  }
  else if (!hasValidFix)
  {
    Serial.printf("[GPS TEST] UART data is arriving. Last data %lu ms ago.\n", now - lastDataSeenMs);
    Serial.println("[GPS TEST] But there is still no valid fix.");
    Serial.println("[GPS TEST] If you also see ANTENNA OPEN/SHORT, solve antenna issue first.");
    Serial.println("[GPS TEST] If only RMC invalid and many empty fields, keep testing outdoors.");
  }
  else
  {
    Serial.printf("[GPS TEST] Latest valid fix UTC=%s lat=%.6f lon=%.6f\n",
                  latestUtcTime.c_str(),
                  latestLatitude,
                  latestLongitude);
  }

  Serial.println("------------------------------------------");
  Serial.println();
}

void autoSwitchBaudIfNeeded()
{
  if (baudLocked)
  {
    return;
  }

  unsigned long now = millis();
  if (now - lastBaudSwitchMs < BAUD_SWITCH_INTERVAL_MS)
  {
    return;
  }

  lastBaudSwitchMs = now;
  currentBaudIndex = (currentBaudIndex + 1) % (sizeof(GPS_BAUD_CANDIDATES) / sizeof(GPS_BAUD_CANDIDATES[0]));
  Serial.printf("[GPS TEST] No valid NMEA yet, switching baud to %lu...\n", currentBaud());
  startGpsUart(currentBaud());
}
} // namespace

void setup()
{
  Serial.begin(115200);
  delay(500);

  bootTimeMs = millis();
  printBanner();
  printWiringHint();

  gpsSerial.setRxBufferSize(GPS_RX_BUFFER_SIZE);
  Serial.println("[GPS TEST] Starting UART1 baud scan...");
  Serial.flush();

  startGpsUart(currentBaud());
  Serial.println("[GPS TEST] Waiting for raw NMEA data...");
}

void loop()
{
  readGpsStream();
  autoSwitchBaudIfNeeded();
  printPeriodicStatus();
  delay(10);
}
