#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ArduinoJson.h>

#include "config.h"

void initWiFi();
String sendTextToServer(String text);
bool sendGpsData(double latitude, double longitude);

#endif
