#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "base64.h"

#if defined(__has_include)
#  if __has_include("config.local.h")
#    include "config.local.h"
#  endif
#endif

#define ULTRASONIC_TASK_PRIORITY 3
#define VOICE_TASK_PRIORITY 10
#define BUTTON_TASK_PRIORITY 1
#define LIGHT_SENSOR_TASK_PRIORITY 1
#define GPS_TASK_PRIORITY 1

#define ULTRASONIC_TASK_STACK_SIZE 4096
#define BUTTON_TASK_STACK_SIZE 2048
#define LIGHT_SENSOR_TASK_STACK_SIZE 4096
#define VOICE_TASK_STACK_SIZE (1024 * 32)
#define GPS_TASK_STACK_SIZE 4096

#define VIBRATION_MODULE_PIN 3
#define ULTRASONIC_TRIG_PIN 8
#define ULTRASONIC_ECHO_PIN 18
#define LIGHT_SENSOR_DO_PIN 10
#define LIGHT_CONTROL_PIN 21
#define BUZZER_PIN 11
#define TEST_BUTTON_PIN 2

#define GPS_RX_PIN 17
#define GPS_TX_PIN -1
#define GPS_BAUD_RATE 9600
#define GPS_UPLOAD_INTERVAL_MS 5000
#define GPS_FIX_STALE_MS 15000

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef SERVER_BASE_URL
#define SERVER_BASE_URL "http://YOUR_SERVER_API_HOST:12345"
#endif

#ifndef DEVICE_ID
#define DEVICE_ID "YOUR_GUIDE_CANE_DEVICE_ID"
#endif

#define GPS_TEST_MODE 0

#ifndef BAIDU_CLIENT_ID
#define BAIDU_CLIENT_ID "YOUR_BAIDU_API_CLIENT_ID"
#endif

#ifndef BAIDU_CLIENT_SECRET
#define BAIDU_CLIENT_SECRET "YOUR_BAIDU_API_CLIENT_SECRET"
#endif

#ifndef BAIDU_CUID
#define BAIDU_CUID "YOUR_BAIDU_CUID"
#endif
#define BAIDU_TOKEN_CLIENT_TIMEOUT_SEC 8
#define BAIDU_TOKEN_CONNECT_TIMEOUT_MS 5000
#define BAIDU_TOKEN_HTTP_TIMEOUT_MS 8000
#define BAIDU_TOKEN_DNS_RETRY_COUNT 2
#define BAIDU_TOKEN_DNS_RETRY_DELAY_MS 250
#define BAIDU_TOKEN_WIFI_WAIT_TIMEOUT_MS 10000
#define BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS 800

#define OBSTACLE_DISTANCE_THRESHOLD 20
#define HIGH_PRIORITY_DISTANCE 8
#define MEDIUM_PRIORITY_DISTANCE 15

#define ULTRASONIC_DEBUG_MODE true
#define ULTRASONIC_MAX_DISTANCE 500
#define ULTRASONIC_MIN_DISTANCE 0.5

#define INMP441_WS 5
#define INMP441_SCK 4
#define INMP441_SD 6

#define MAX98357_LRC 16
#define MAX98357_BCLK 15
#define MAX98357_DIN 7
#define I2S_IN_PORT I2S_NUM_0

#define SAMPLE_RATE 16000
#define RECORD_TIME_SECONDS 10
#define BUFFER_SIZE (SAMPLE_RATE * RECORD_TIME_SECONDS)

#define EIDSP_QUANTIZE_FILTERBANK 0
#define LED_BUILT_IN 21

extern bool isConnectedToWifi;

extern TaskHandle_t ultrasonicTaskHandle;
extern TaskHandle_t buttonTaskHandle;
extern TaskHandle_t lightSensorTaskHandle;
extern TaskHandle_t voiceTaskHandle;
extern TaskHandle_t gpsTaskHandle;

extern bool record_status_me;
extern String accessToken;

#endif // CONFIG_H
