#include "gps.h"

#include <HardwareSerial.h>
#include <string.h>

#include "config.h"
#include "network.h"

namespace
{
HardwareSerial gpsSerial(1);
GpsFix latestFix = {0.0, 0.0, false, 0, ""};
char sentenceBuffer[128];
size_t sentenceIndex = 0;

double nmeaToDecimal(const char *value)
{
    double raw = atof(value);
    int degrees = static_cast<int>(raw / 100);
    double minutes = raw - (degrees * 100);
    return degrees + minutes / 60.0;
}

void updateFix(const char *utcTime, const char *latValue, const char *ns, const char *lonValue, const char *ew)
{
    if (!latValue || !lonValue || !ns || !ew || !utcTime)
    {
        return;
    }

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

    latestFix.latitude = latitude;
    latestFix.longitude = longitude;
    latestFix.valid = true;
    latestFix.updatedAt = millis();
    latestFix.utcTime = utcTime;

    Serial.printf("[GPS] Valid fix parsed | lat=%.6f lon=%.6f utc=%s\n",
                  latestFix.latitude, latestFix.longitude, latestFix.utcTime.c_str());
}

void parseRmcSentence(char *sentence)
{
    char *fields[16] = {0};
    int fieldCount = 0;

    char *context = nullptr;
    char *token = strtok_r(sentence, ",", &context);
    while (token != nullptr && fieldCount < 16)
    {
        fields[fieldCount++] = token;
        token = strtok_r(nullptr, ",", &context);
    }

    if (fieldCount < 7)
    {
        return;
    }

    if (strcmp(fields[2], "A") != 0)
    {
        latestFix.valid = false;
        return;
    }

    updateFix(fields[1], fields[3], fields[4], fields[5], fields[6]);
}

void processSentence(char *sentence)
{
    if (sentence[0] != '$')
    {
        return;
    }

    if (strlen(sentence) < 6)
    {
        return;
    }

    if (sentence[3] == 'R' && sentence[4] == 'M' && sentence[5] == 'C')
    {
        parseRmcSentence(sentence);
    }
}
} // namespace

void gpsInit()
{
    Serial.printf("[GPS] Initializing GPS module...\n");
    Serial.printf("[GPS] About to start UART1 with RX=%d, TX=%d, baud=%d\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    Serial.flush();

    gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    sentenceIndex = 0;
    latestFix = {0.0, 0.0, false, 0, ""};

    Serial.printf("[GPS] UART1 started successfully | RX=%d TX=%d baud=%d\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    Serial.flush();
}

void gpsPoll()
{
    while (gpsSerial.available() > 0)
    {
        char c = static_cast<char>(gpsSerial.read());

        if (c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            sentenceBuffer[sentenceIndex] = '\0';
            if (sentenceIndex > 0)
            {
                processSentence(sentenceBuffer);
            }
            sentenceIndex = 0;
            continue;
        }

        if (sentenceIndex < sizeof(sentenceBuffer) - 1)
        {
            sentenceBuffer[sentenceIndex++] = c;
        }
        else
        {
            sentenceIndex = 0;
        }
    }
}

bool gpsHasValidFix()
{
    return latestFix.valid && (millis() - latestFix.updatedAt) <= GPS_FIX_STALE_MS;
}

GpsFix gpsGetLatestFix()
{
    return latestFix;
}

void gpsTask(void *pvParameters)
{
    (void)pvParameters;
    unsigned long lastUploadTime = 0;
    unsigned long lastWaitingLogTime = 0;

    Serial.printf("[GPS] Task started | UART1 RX=%d TX=%d baud=%d upload=%dms\n",
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE, GPS_UPLOAD_INTERVAL_MS);

    while (true)
    {
        gpsPoll();

        if (gpsHasValidFix() && isConnectedToWifi)
        {
            unsigned long now = millis();
            if (now - lastUploadTime >= GPS_UPLOAD_INTERVAL_MS)
            {
                GpsFix fix = gpsGetLatestFix();
                if (sendGpsData(fix.latitude, fix.longitude))
                {
                    lastUploadTime = now;
                    Serial.printf("[GPS] Upload success | lat=%.6f lon=%.6f\n", fix.latitude, fix.longitude);
                }
                else
                {
                    Serial.printf("[GPS] Upload failed | lat=%.6f lon=%.6f\n", fix.latitude, fix.longitude);
                }
            }
        }
        else if (millis() - lastWaitingLogTime >= 5000)
        {
            lastWaitingLogTime = millis();
            if (!isConnectedToWifi)
            {
                Serial.println("[GPS] Waiting for WiFi before uploading fixes...");
            }
            else
            {
                Serial.println("[GPS] Waiting for a fresh valid NMEA fix...");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
