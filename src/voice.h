#ifndef VOICE_H
#define VOICE_H

#include <Arduino.h>
#include <string.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UrlEncode.h>

#include "config.h"

void set_i2s();
String getAccessToken_baidu();
String waitForAccessToken_baidu();
String baidu_voice_recognition(String accessToken, uint8_t *audioData, int audioDataSize);
void baiduTTS_Send(String access_token, String text);

#endif // VOICE_H
