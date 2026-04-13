#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>

#include "app_config.h"
#include "app_httpd.h"
#include "camera_pins.h"

namespace
{
camera_config_t buildCameraConfig()
{
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = CAMERA_PIXEL_FORMAT;
    config.frame_size = CAMERA_FRAME_SIZE;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;
    config.fb_count = CAMERA_FB_COUNT;
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (!psramFound())
    {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
    }

    return config;
}

bool initCamera()
{
    camera_config_t config = buildCameraConfig();
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("[CAM] Camera init failed: 0x%x\n", err);
        return false;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != nullptr)
    {
        sensor->set_framesize(sensor, config.frame_size);
        sensor->set_vflip(sensor, 1);
        sensor->set_brightness(sensor, 1);
        sensor->set_saturation(sensor, -1);
    }

    Serial.println("[CAM] Camera initialized");
    return true;
}

bool connectWifi()
{
    Serial.printf("[NET] Connecting to hotspot SSID: %s\n", CAMERA_WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(CAMERA_HOSTNAME);
    WiFi.begin(CAMERA_WIFI_SSID, CAMERA_WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[NET] WiFi connect failed");
        return false;
    }

    Serial.println("[NET] WiFi connected");
    Serial.printf("[NET] IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[NET] Open in browser: http://%s/\n", WiFi.localIP().toString().c_str());
    Serial.printf("[NET] Stream URL: http://%s:81/stream\n", WiFi.localIP().toString().c_str());
    Serial.printf("[NET] Capture URL: http://%s/capture\n", WiFi.localIP().toString().c_str());
    return true;
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32-CAM PlatformIO Stream Server");
    Serial.println("Board: AI Thinker ESP32-CAM");
    Serial.println("========================================");

    pinMode(CAMERA_FLASH_LED_PIN, OUTPUT);
    digitalWrite(CAMERA_FLASH_LED_PIN, LOW);

    if (!initCamera())
    {
        Serial.println("[CAM] Fatal: camera initialization failed");
        return;
    }

    if (!connectWifi())
    {
        Serial.println("[NET] Fatal: hotspot connection failed");
        return;
    }

    if (!startCameraServer())
    {
        Serial.println("[HTTP] Fatal: camera server start failed");
        return;
    }

    Serial.println("[HTTP] Camera HTTP server started");
}

void loop()
{
    delay(1000);
}
