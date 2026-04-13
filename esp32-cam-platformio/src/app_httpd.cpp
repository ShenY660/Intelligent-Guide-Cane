#include "app_httpd.h"

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace
{
httpd_handle_t cameraHttpd = nullptr;
httpd_handle_t streamHttpd = nullptr;

constexpr char PART_BOUNDARY[] = "123456789000000000000987654321";
constexpr char STREAM_CONTENT_TYPE[] = "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
constexpr char STREAM_BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
constexpr char STREAM_PART[] = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %lld\r\n\r\n";

esp_err_t rootHandler(httpd_req_t *req)
{
    const char html[] =
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ESP32-CAM</title></head><body style='font-family:sans-serif;background:#111;color:#eee;'>"
        "<h2>ESP32-CAM Stream Ready</h2>"
        "<p>MJPEG stream endpoint: <a href='/stream'>/stream</a></p>"
        "<p>Capture endpoint: <a href='/capture'>/capture</a></p>"
        "<img src='/stream' style='width:100%;max-width:640px;border-radius:8px;' />"
        "</body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t healthHandler(httpd_req_t *req)
{
    sensor_t *sensor = esp_camera_sensor_get();
    char buffer[256];
    snprintf(
        buffer,
        sizeof(buffer),
        "{\"ok\":true,\"stream_port\":81,\"capture\":\"/capture\",\"stream\":\"/stream\",\"pixformat\":%u,\"framesize\":%u}",
        sensor ? sensor->pixformat : 0,
        sensor ? sensor->status.framesize : 0);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buffer, HTTPD_RESP_USE_STRLEN);
}

esp_err_t captureHandler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    uint8_t *jpegBuffer = nullptr;
    size_t jpegLength = 0;
    bool converted = true;

    if (fb->format == PIXFORMAT_JPEG)
    {
        jpegBuffer = fb->buf;
        jpegLength = fb->len;
    }
    else
    {
        converted = frame2jpg(fb, 80, &jpegBuffer, &jpegLength);
    }

    if (!converted)
    {
        esp_camera_fb_return(fb);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t result = httpd_resp_send(req, reinterpret_cast<const char *>(jpegBuffer), jpegLength);

    if (fb->format != PIXFORMAT_JPEG && jpegBuffer)
    {
        free(jpegBuffer);
    }
    esp_camera_fb_return(fb);
    return result;
}

esp_err_t streamHandler(httpd_req_t *req)
{
    httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    while (true)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            return ESP_FAIL;
        }

        uint8_t *jpegBuffer = nullptr;
        size_t jpegLength = 0;
        bool converted = true;

        if (fb->format == PIXFORMAT_JPEG)
        {
            jpegBuffer = fb->buf;
            jpegLength = fb->len;
        }
        else
        {
            converted = frame2jpg(fb, 80, &jpegBuffer, &jpegLength);
        }

        if (!converted)
        {
            esp_camera_fb_return(fb);
            return ESP_FAIL;
        }

        char header[128];
        const long long timestampUs = esp_timer_get_time();
        size_t headerLength = snprintf(header, sizeof(header), STREAM_PART, jpegLength, timestampUs);

        esp_err_t result = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (result == ESP_OK)
        {
            result = httpd_resp_send_chunk(req, header, headerLength);
        }
        if (result == ESP_OK)
        {
            result = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(jpegBuffer), jpegLength);
        }

        if (fb->format != PIXFORMAT_JPEG && jpegBuffer)
        {
            free(jpegBuffer);
        }
        esp_camera_fb_return(fb);

        if (result != ESP_OK)
        {
            break;
        }
    }

    return ESP_FAIL;
}
} // namespace

bool startCameraServer()
{
    httpd_config_t cameraConfig = HTTPD_DEFAULT_CONFIG();
    cameraConfig.server_port = 80;
    cameraConfig.ctrl_port = 32768;
    cameraConfig.max_uri_handlers = 8;

    httpd_uri_t rootUri = {};
    rootUri.uri = "/";
    rootUri.method = HTTP_GET;
    rootUri.handler = rootHandler;
    rootUri.user_ctx = nullptr;

    httpd_uri_t healthUri = {};
    healthUri.uri = "/health";
    healthUri.method = HTTP_GET;
    healthUri.handler = healthHandler;
    healthUri.user_ctx = nullptr;

    httpd_uri_t captureUri = {};
    captureUri.uri = "/capture";
    captureUri.method = HTTP_GET;
    captureUri.handler = captureHandler;
    captureUri.user_ctx = nullptr;

    if (httpd_start(&cameraHttpd, &cameraConfig) != ESP_OK)
    {
        return false;
    }

    httpd_register_uri_handler(cameraHttpd, &rootUri);
    httpd_register_uri_handler(cameraHttpd, &healthUri);
    httpd_register_uri_handler(cameraHttpd, &captureUri);

    httpd_config_t streamConfig = HTTPD_DEFAULT_CONFIG();
    streamConfig.server_port = 81;
    streamConfig.ctrl_port = 32769;
    streamConfig.max_uri_handlers = 4;

    httpd_uri_t streamUri = {};
    streamUri.uri = "/stream";
    streamUri.method = HTTP_GET;
    streamUri.handler = streamHandler;
    streamUri.user_ctx = nullptr;
    if (httpd_start(&streamHttpd, &streamConfig) != ESP_OK)
    {
        return false;
    }

    httpd_register_uri_handler(streamHttpd, &streamUri);
    return true;
}
