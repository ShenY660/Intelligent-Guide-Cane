#include "voice.h"

i2s_config_t i2sIn_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512};

const i2s_pin_config_t i2sIn_pin_config = {
    .bck_io_num = INMP441_SCK,
    .ws_io_num = INMP441_WS,
    .data_out_num = -1,
    .data_in_num = INMP441_SD};

i2s_config_t i2sOut_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 1024};

const i2s_pin_config_t i2sOut_pin_config = {
    .bck_io_num = MAX98357_BCLK,
    .ws_io_num = MAX98357_LRC,
    .data_out_num = MAX98357_DIN,
    .data_in_num = -1};

// 初始化I2S
void set_i2s()
{
  esp_err_t err;
  
  // 先卸载可能存在的I2S驱动
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_1);
  
  // 添加延迟确保驱动完全卸载
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // 初始化I2S输入 (麦克风)
  Serial.println("正在初始化I2S输入驱动...");
  err = i2s_driver_install(I2S_NUM_0, &i2sIn_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S输入驱动安装失败: %s\n", esp_err_to_name(err));
    return;
  }
  
  err = i2s_set_pin(I2S_NUM_0, &i2sIn_pin_config);
  if (err != ESP_OK) {
    Serial.printf("I2S输入引脚配置失败: %s\n", esp_err_to_name(err));
    return;
  }
  Serial.println("I2S输入驱动初始化成功");
  
  // 添加延迟
  vTaskDelay(pdMS_TO_TICKS(50));
  
  // 初始化I2S输出 (扬声器)
  Serial.println("正在初始化I2S输出驱动...");
  err = i2s_driver_install(I2S_NUM_1, &i2sOut_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S输出驱动安装失败: %s\n", esp_err_to_name(err));
    return;
  }
  
  err = i2s_set_pin(I2S_NUM_1, &i2sOut_pin_config);
  if (err != ESP_OK) {
    Serial.printf("I2S输出引脚配置失败: %s\n", esp_err_to_name(err));
    return;
  }
  Serial.println("I2S输出驱动初始化成功");
  
  // 清空DMA缓冲区
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_zero_dma_buffer(I2S_NUM_1);
  
  Serial.println("I2S音频系统初始化完成");
}

// 播放声音
// audioData: PCM原始数据的指针
// audioDataSize: size of PCM data
void playAudio(uint8_t *audioData, size_t audioDataSize)
{
  if (audioDataSize > 0)
  {
    size_t bytes_written = 0;
    size_t total_written = 0;
    
    // 确保所有数据都被写入
    while (total_written < audioDataSize)
    {
      esp_err_t result = i2s_write(I2S_NUM_1, 
                                   (int16_t *)(audioData + total_written), 
                                   audioDataSize - total_written, 
                                   &bytes_written, 
                                   pdMS_TO_TICKS(100));
      
      if (result != ESP_OK)
      {
        Serial.printf("[音频播放] I2S写入错误: %s\n", esp_err_to_name(result));
        break;
      }
      
      total_written += bytes_written;
      
      // 如果没有写入任何数据，稍作延迟避免忙等待
      if (bytes_written == 0)
      {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
}

// 清空I2S DMA缓冲区
void clearAudio(void)
{
  // 清空I2S DMA缓冲区
  i2s_zero_dma_buffer(I2S_NUM_1);
  Serial.print("clearAudio");
}

static bool downloadHttpBody(HTTPClient &http, uint8_t **outBuffer, size_t *outLength)
{
  if (outBuffer == nullptr || outLength == nullptr)
  {
    return false;
  }

  *outBuffer = nullptr;
  *outLength = 0;

  WiFiClient *stream = http.getStreamPtr();
  if (stream == nullptr)
  {
    return false;
  }

  int contentLength = http.getSize();
  size_t capacity = contentLength > 0 ? static_cast<size_t>(contentLength) : 4096;
  uint8_t *buffer = static_cast<uint8_t *>(malloc(capacity));
  if (buffer == nullptr)
  {
    Serial.println("[语音合成] 错误: 无法分配音频下载缓冲区");
    return false;
  }

  size_t totalRead = 0;
  unsigned long lastDataTime = millis();

  while (http.connected() && (contentLength > 0 || contentLength == -1))
  {
    size_t availableBytes = stream->available();
    if (availableBytes == 0)
    {
      if (millis() - lastDataTime > 3000)
      {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    if (totalRead + availableBytes > capacity)
    {
      size_t newCapacity = capacity;
      while (totalRead + availableBytes > newCapacity)
      {
        newCapacity *= 2;
      }
      uint8_t *newBuffer = static_cast<uint8_t *>(realloc(buffer, newCapacity));
      if (newBuffer == nullptr)
      {
        free(buffer);
        Serial.println("[语音合成] 错误: 扩展音频缓冲区失败");
        return false;
      }
      buffer = newBuffer;
      capacity = newCapacity;
    }

    int bytesRead = stream->readBytes(buffer + totalRead, availableBytes);
    if (bytesRead <= 0)
    {
      break;
    }

    totalRead += static_cast<size_t>(bytesRead);
    lastDataTime = millis();

    if (contentLength > 0)
    {
      contentLength -= bytesRead;
    }
  }

  if (totalRead == 0)
  {
    free(buffer);
    Serial.println("[语音合成] 错误: 未下载到任何音频数据");
    return false;
  }

  *outBuffer = buffer;
  *outLength = totalRead;
  return true;
}

static bool getWavPayloadRange(const uint8_t *buffer, size_t bufferLength, size_t *payloadOffset, size_t *payloadLength)
{
  if (payloadOffset == nullptr || payloadLength == nullptr || buffer == nullptr || bufferLength < 12)
  {
    return false;
  }

  if (memcmp(buffer, "RIFF", 4) != 0 || memcmp(buffer + 8, "WAVE", 4) != 0)
  {
    *payloadOffset = 0;
    *payloadLength = bufferLength;
    return true;
  }

  size_t offset = 12;
  while (offset + 8 <= bufferLength)
  {
    const uint8_t *chunkId = buffer + offset;
    uint32_t chunkSize = static_cast<uint32_t>(buffer[offset + 4]) |
                         (static_cast<uint32_t>(buffer[offset + 5]) << 8) |
                         (static_cast<uint32_t>(buffer[offset + 6]) << 16) |
                         (static_cast<uint32_t>(buffer[offset + 7]) << 24);

    if (memcmp(chunkId, "data", 4) == 0)
    {
      size_t dataOffset = offset + 8;
      if (dataOffset > bufferLength)
      {
        return false;
      }

      size_t dataLength = min(static_cast<size_t>(chunkSize), bufferLength - dataOffset);
      *payloadOffset = dataOffset;
      *payloadLength = dataLength;
      return true;
    }

    offset += 8 + chunkSize + (chunkSize & 0x01);
  }

  return false;
}


// 获取百度云平台的AccessToken
static bool waitForBaiduWiFiConnection(uint32_t timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.printf("[语音服务] WiFi 未连接，准备重新连接到 %s\n", WIFI_SSID);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  unsigned long lastLogMs = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs)
  {
    delay(250);

    if (millis() - lastLogMs >= 1000)
    {
      Serial.printf("[语音服务] 等待 WiFi 连通中... status=%d elapsed=%lu ms\n",
                    WiFi.status(),
                    millis() - startMs);
      lastLogMs = millis();
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("[语音服务] WiFi 已连接，IP=%s DNS1=%s DNS2=%s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str());
    return true;
  }

  Serial.printf("[语音服务] WiFi 仍未连接，status=%d，稍后继续重试\n", WiFi.status());
  return false;
}

#if 0
static String getAccessToken_baidu_legacy()
{
  Serial.println("[语音服务] 开始获取百度AccessToken...");

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[语音服务] 错误：当前WiFi未连接，无法获取百度AccessToken");
    return "";
  }
  
  // 创建安全客户端
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client)
  {
    Serial.println("[语音服务] 错误：无法创建HTTPS客户端");
    return "";
  }
  
  // 跳过证书验证
  client->setInsecure();
  
  // 设置连接超时
  client->setTimeout(BAIDU_TOKEN_CLIENT_TIMEOUT_SEC);
  
  // 创建HTTP客户端
  HTTPClient https;
  HTTPClient https;
  String accessToken = "";
  const char *baiduHost = "aip.baidubce.com";
  IPAddress resolvedIp;
  unsigned long requestStartMs = millis();

  // 百度API请求地址
  String target_url = String("https://aip.baidubce.com/oauth/2.0/token?client_id=") + BAIDU_CLIENT_ID + "&client_secret=" + BAIDU_CLIENT_SECRET + "&grant_type=client_credentials";

  bool dnsResolved = false;
  for (int attempt = 1; attempt <= BAIDU_TOKEN_DNS_RETRY_COUNT; ++attempt)
  {
    Serial.printf("[语音服务] 正在解析域名 %s，第 %d/%d 次...\n",
                  baiduHost, attempt, BAIDU_TOKEN_DNS_RETRY_COUNT);
    if (WiFi.hostByName(baiduHost, resolvedIp))
    {
      dnsResolved = true;
      Serial.printf("[语音服务] 域名解析成功: %s -> %s\n",
                    baiduHost, resolvedIp.toString().c_str());
      break;
    }

    Serial.printf("[语音服务] 域名解析失败，第 %d/%d 次，%d ms 后重试\n",
                  attempt, BAIDU_TOKEN_DNS_RETRY_COUNT, BAIDU_TOKEN_DNS_RETRY_DELAY_MS);
    delay(BAIDU_TOKEN_DNS_RETRY_DELAY_MS);
  }

  if (!dnsResolved)
  {
    Serial.println("[语音服务] 错误：百度域名解析持续失败，请检查热点/路由器 DNS 或外网连通性");
    delete client;
    return "";
  }

  Serial.println("[语音服务] 连接到百度API服务器...");
  #if 0
  if (!https.begin(*client, target_url))
  {
    #if 0
    #if 0
    Serial.println("[语音服务] 错误：HTTPClient 初始化失败");
    #endif
    Serial.println("[voice] HTTPClient init failed");
    #endif
    Serial.println("[voice] HTTPClient init failed");
    delete client;
    return "";
  }
  
  // 设置HTTP超时
  #endif
  if (!https.begin(*client, target_url))
  {
    #if 0
    Serial.println("[语音服务] 错误：HTTPClient 初始化失败");
    #endif
    Serial.println("[voice] HTTPClient init failed");
    delete client;
    return "";
  }
  https.setConnectTimeout(BAIDU_TOKEN_CONNECT_TIMEOUT_MS);
  https.setTimeout(BAIDU_TOKEN_HTTP_TIMEOUT_MS);
  
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Accept", "application/json");

  // 构造空JSON字符串负载
  String payload = "\"\"";
  
  Serial.printf("[语音服务] 等待百度Token响应，客户端超时=%d秒，HTTP超时=%d毫秒\n",
                BAIDU_TOKEN_CLIENT_TIMEOUT_SEC, BAIDU_TOKEN_HTTP_TIMEOUT_MS);
  int httpCode = https.POST(payload);
  
  // Serial.printf("[语音服务] HTTP响应代码: %d\n", httpCode);

  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
    {
      String response = https.getString();
      // Serial.println("[语音服务] 收到响应: " + response);
      
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      
      if (error) {
        Serial.printf("[语音服务] JSON解析错误: %s\n", error.c_str());
      } else {
        accessToken = doc["access_token"].as<String>();
        if (accessToken.length() > 0) {
          Serial.printf("[语音服务] AccessToken 获取成功，耗时 %lu ms\n", millis() - requestStartMs);
          Serial.println("[语音服务] ✓ 获取AccessToken成功");
        } else {
          Serial.printf("[语音服务] 错误：响应中没有access_token，body=%s\n", response.c_str());
          Serial.println("[语音服务] 错误：响应中没有access_token");
        }
      }
    }
    else
    {
      String response = https.getString();
      Serial.printf("[语音服务] HTTP 请求失败，错误代码: %d, 错误信息: %s, body=%s\n",
                    httpCode, https.errorToString(httpCode).c_str(), response.c_str());
      Serial.printf("[语音服务] HTTP请求失败，错误代码: %d, 错误信息: %s\n", 
                    httpCode, https.errorToString(httpCode).c_str());
    }
  }
  else
  {
    Serial.printf("[语音服务] 连接失败，错误代码: %d, 错误信息: %s\n", 
                  httpCode, https.errorToString(httpCode).c_str());
  }
  
  https.end();
  delete client;
  
  if (accessToken.length() == 0) {
    Serial.println("[语音服务] ✗ AccessToken获取失败，语音服务将无法使用");
  }
  Serial.println("token为:" + accessToken);

  return accessToken;
}

// 百度语音识别
// accessToken: 百度云平台的AccessToken
// audioData: PCM原始数据的指针
// audioDataSize: size of PCM data
static String waitForAccessToken_baidu_legacy()
{
  uint32_t attempt = 0;

  while (true)
  {
    ++attempt;
    Serial.printf("[语音服务] 阻塞等待百度AccessToken，第 %lu 次尝试\n",
                  static_cast<unsigned long>(attempt));

    if (!waitForBaiduWiFiConnection(BAIDU_TOKEN_WIFI_WAIT_TIMEOUT_MS))
    {
      delay(BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
      continue;
    }

    String token = getAccessToken_baidu();
    if (token.length() > 0)
    {
      return token;
    }

    Serial.printf("[语音服务] AccessToken 仍未获取到，%d ms 后继续阻塞重试\n",
                  BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
    delay(BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
  }
}

#endif
static bool ensureWiFiForBaiduToken(uint32_t timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.printf("[voice] WiFi not connected, reconnecting to %s\n", WIFI_SSID);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs)
  {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("[voice] WiFi ready, ip=%s dns1=%s dns2=%s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str());
    return true;
  }

  Serial.printf("[voice] WiFi still not ready, status=%d\n", WiFi.status());
  return false;
}

String getAccessToken_baidu()
{
  if (!ensureWiFiForBaiduToken(BAIDU_TOKEN_WIFI_WAIT_TIMEOUT_MS))
  {
    return "";
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(BAIDU_TOKEN_CLIENT_TIMEOUT_SEC);

  HTTPClient https;
  IPAddress resolvedIp;
  const char *baiduHost = "aip.baidubce.com";
  String accessToken = "";
  String targetUrl =
      String("https://aip.baidubce.com/oauth/2.0/token?client_id=") +
      BAIDU_CLIENT_ID +
      "&client_secret=" + BAIDU_CLIENT_SECRET +
      "&grant_type=client_credentials";

  bool dnsResolved = false;
  for (int attempt = 1; attempt <= BAIDU_TOKEN_DNS_RETRY_COUNT; ++attempt)
  {
    Serial.printf("[voice] resolving %s (%d/%d)\n",
                  baiduHost,
                  attempt,
                  BAIDU_TOKEN_DNS_RETRY_COUNT);

    if (WiFi.hostByName(baiduHost, resolvedIp))
    {
      dnsResolved = true;
      Serial.printf("[voice] dns ok: %s -> %s\n",
                    baiduHost,
                    resolvedIp.toString().c_str());
      break;
    }

    delay(BAIDU_TOKEN_DNS_RETRY_DELAY_MS);
  }

  if (!dnsResolved)
  {
    Serial.println("[voice] dns resolve failed");
    return "";
  }

  if (!https.begin(client, targetUrl))
  {
    Serial.println("[voice] HTTPClient begin failed");
    return "";
  }

  https.setConnectTimeout(BAIDU_TOKEN_CONNECT_TIMEOUT_MS);
  https.setTimeout(BAIDU_TOKEN_HTTP_TIMEOUT_MS);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Accept", "application/json");

  unsigned long requestStartMs = millis();
  int httpCode = https.POST("");

  if (httpCode > 0)
  {
    String response = https.getString();

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
    {
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      if (!error)
      {
        accessToken = doc["access_token"].as<String>();
      }
      else
      {
        Serial.printf("[voice] token json parse failed: %s\n", error.c_str());
      }

      if (accessToken.length() > 0)
      {
        Serial.printf("[voice] token ok in %lu ms\n", millis() - requestStartMs);
      }
      else
      {
        Serial.printf("[voice] token missing in response: %s\n", response.c_str());
      }
    }
    else
    {
      Serial.printf("[voice] token http failed: code=%d err=%s body=%s\n",
                    httpCode,
                    https.errorToString(httpCode).c_str(),
                    response.c_str());
    }
  }
  else
  {
    Serial.printf("[voice] token request failed: code=%d err=%s\n",
                  httpCode,
                  https.errorToString(httpCode).c_str());
  }

  https.end();
  return accessToken;
}

String waitForAccessToken_baidu()
{
  uint32_t attempt = 0;

  while (true)
  {
    ++attempt;
    Serial.printf("[voice] waiting for Baidu token, attempt=%lu\n",
                  static_cast<unsigned long>(attempt));

    String token = getAccessToken_baidu();
    if (token.length() > 0)
    {
      return token;
    }

    Serial.printf("[voice] token unavailable, retry in %d ms\n",
                  BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
    delay(BAIDU_TOKEN_BLOCK_RETRY_DELAY_MS);
  }
}

String baidu_voice_recognition(String accessToken, uint8_t *audioData, int audioDataSize)
{
  String recognizedText = "";

  if (accessToken == "")
  {
    Serial.println("access_token is null");
    return recognizedText;
  }

  // audio数据包许愿哦进行Base64编码，数据量会增大1/3
  int audio_data_len = audioDataSize * sizeof(char) * 1.4;
  unsigned char *audioDataBase64 = (unsigned char *)ps_malloc(audio_data_len);
  if (!audioDataBase64)
  {
    Serial.println("Failed to allocate memory for audioDataBase64");
    return recognizedText;
  }

  // json包大小，由于需要将audioData数据进行Base64的编码，数据量会增大1/3
  int data_json_len = audioDataSize * sizeof(char) * 1.4;
  char *data_json = (char *)ps_malloc(data_json_len);
  if (!data_json)
  {
    Serial.println("Failed to allocate memory for data_json");
    return recognizedText;
  }

  // Base64 encode audio data
  encode_base64(audioData, audioDataSize, audioDataBase64);

  memset(data_json, '\0', data_json_len);
  strcat(data_json, "{");
  strcat(data_json, "\"format\":\"pcm\",");
  strcat(data_json, "\"rate\":16000,");
  strcat(data_json, "\"dev_pid\":1537,");
  strcat(data_json, "\"channel\":1,");
  strcat(data_json, "\"cuid\":\"57722200\",");
  strcat(data_json, "\"token\":\"");
  strcat(data_json, accessToken.c_str());
  strcat(data_json, "\",");
  sprintf(data_json + strlen(data_json), "\"len\":%d,", audioDataSize);
  strcat(data_json, "\"speech\":\"");
  strcat(data_json, (const char *)audioDataBase64);
  strcat(data_json, "\"");
  strcat(data_json, "}");

  // 创建http请求
  HTTPClient http_client;

  http_client.begin("http://vop.baidu.com/server_api");
  http_client.addHeader("Content-Type", "application/json");
  int httpCode = http_client.POST(data_json);

  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK)
    {
      // 获取返回结果
      String response = http_client.getString();
      Serial.println(response);

      // 从json中解析对应的result
      DynamicJsonDocument responseDoc(1024);
      deserializeJson(responseDoc, response);

      // 假设 responseDoc["result"] 是一个 JSON 数组，并且你想要第一个元素
      if (responseDoc["result"].is<JsonArray>())
      {
        JsonArray resultArray = responseDoc["result"].as<JsonArray>();
        if (resultArray.size() > 0)
        {
          recognizedText = resultArray[0].as<String>();
        }
        else
        {
          Serial.println("Error: 'result' 数组为空。");
          recognizedText = ""; // 或者其他你希望的默认值
        }
      }
      else if (responseDoc["result"].is<String>())
      {
        // 如果 "result" 直接就是一个字符串，则直接赋值
        recognizedText = responseDoc["result"].as<String>();
      }
      else
      {
        Serial.println("Error: 'result' 不是一个字符串或字符串数组。");
        recognizedText = ""; // 或者其他你希望的默认值
      }
    }
  }
  else
  {
    Serial.printf("[HTTP] POST failed, error: %s\n", http_client.errorToString(httpCode).c_str());
  }

  // 释放内存
  if (audioDataBase64)
  {
    free(audioDataBase64);
  }

  if (data_json)
  {
    free(data_json);
  }

  http_client.end();

  return recognizedText;
}

// Play zai audio data using MAX98357A
void playAudio_Zai(void)
{
  const char *zai = "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AAAEAAoACAAKAAwAEgAYACQANAA6AEYAYgB8AIQAhgCUAJ4AmgCSAJQAkgCOAIoAgAB6AHYAcgBoAGQAaABkAF4AYACAAKwA3gAAAQoBCgEAAeoA2gDcAMoAugCmAJwAigBkADYAHAAiACgAHAACAP7/CgAYAC4APgBCAEYAOgAyACwAPgBGAD4AJgACAN7/wP+y/7j/xP/O/9D/vv+q/5T/jP+K/5b/sP/M/+D/5P/U/7r/ov+Y/6L/uv/Y/+z/7P/Y/7r/nv+U/57/sP/G/9D/1v/Q/8L/rv+g/57/pP+0/8r/3P/g/9r/zP/C/8L/0v/u/wIADAAOAAYA/v/2//z/CgAWABgAGgAaABgAEgAOABIAEgAWABgAHgAgABwAFAAOAAwADAAOABIAFAASAA4ACgAGAAIAAAD6//j/8v/y//b/+v/+/wAAAAAAAP7/AAAAAAgADAAQABQADgAMAAwACAACAAoACAAKAA4ACgAGAAAA/v8CABAAGAAYABwAHgASABYAGgAaABoAGAAUAAoAAAD+//b/7v/s/+r/6P/0//L/8v/0//j/AAAAAAQADgAWACAAHgAYABYAFAAQABQAGAAQAAYA/v/4//z//v8AAAIACAAOABIAFAAUABIAEgAWABgAHAAaABQACgAGAAYACAAMABAAFgASABAACAACAAIACAAMABAAEAAIAAIA/P/2//T/+v/8/wQACgAEAP7/8P/q/+b/8P/s/+7/8v/6//T/4v/o/+7/6P/q//L/9v/2//D/5P/m/+r/7v/0//7/AAAAAPj/8P/o/+z/9P/4/wQABAAAAPz/8v/4//r//v8EAAgACgAGAAYABAAAAAQADAAUABoAHAAWABAACgAEABAAFAAcABgAFAAKAAIAAgACAAwACgAQABAAEgAQABAAEgASABgAGAAeACAAHAAgAB4AHgAeABwAIAAeACAAHAAYABgAFAAQAA4ADgAMAAwADAAQABAAEgAUABYAGgAeACIAIgAmACAAIAAeABgAFgAWABIAEgAGAAQABgAEAAYABAAIABQAHAAiACIAIgAiACAAHgAiACgAHgAYABQAEAASAAoACAAEAP7//P/8//7//P/8//z//P/+/wAAAAAEAAgACAAIAAYABAAAAAAAAgAAAPz/+v/2//b/+v/6//b/+P/0//j/+v/4//r/9P/2//T/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AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

  // 分配内存
  uint8_t *decode_data = (uint8_t *)ps_malloc(16000 * 3);
  if (!decode_data)
  {
    Serial.println("Failed to allocate memory for decode_data");
    return;
  }

  // base64 解析
  int decoded_length = decode_base64((unsigned char *)zai, (unsigned char *)decode_data);

  // 播放
  playAudio(decode_data, decoded_length);

  // delay 200ms
  delay(200);

  // 清空I2S DMA缓冲区
  clearAudio();

  // 释放内存
  free(decode_data);
}


void baiduTTS_Send(String access_token, String text)
{
  if (access_token == "")
  {
    Serial.println("access_token is null");
    return;
  }

  if (text.length() == 0)
  {
    Serial.println("text is null");
    return;
  }

  const int per = 1;
  const int spd = 6;
  const int pit = 5;
  const int vol = 9;
  const int aue = 6;

  // 进行 URL 编码
  String encodedText = urlEncode(urlEncode(text));

  // URL http请求数据封装
  String url = "https://tsn.baidu.com/text2audio";

  const char *header[] = {"Content-Type", "Content-Length"};

  url += "?tok=" + access_token; // Token
  url += "&tex=" + encodedText;  //  需要合成的文本，tex字段2次urlencode
  url += "&per=" + String(per);  // 发音人选择, 度小宇=1，度小美=0，度逍遥（基础）=3，度丫丫=4
  url += "&spd=" + String(spd);  // 语速，取值0-15，默认为5中语速
  url += "&pit=" + String(pit);  // 音调，取值0-15，默认为5中语调
  url += "&vol=" + String(vol);  // 音量，取值0-15，默认为5中音量
  url += "&aue=" + String(aue);  // 3为mp3格式(默认)； 4为pcm-16k；5为pcm-8k；6为wav（内容同pcm-16k）; 注意aue=4或者6是语音识别要求的格式，但是音频内容不是语音识别要求的自然人发音，所以识别效果会受影响。
  url += "&cuid=" + String(BAIDU_CUID);
  url += "&lan=zh";
  url += "&ctp=1";

  // http请求创建
  HTTPClient http;

  http.begin(url);
  http.collectHeaders(header, 2);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    if (httpResponseCode == HTTP_CODE_OK)
    {
      String contentType = http.header("Content-Type");
      if (contentType.startsWith("audio"))
      {
        Serial.println("合成成功");
        uint8_t *audioBuffer = nullptr;
        size_t audioLength = 0;
        if (downloadHttpBody(http, &audioBuffer, &audioLength))
        {
          size_t payloadOffset = 0;
          size_t payloadLength = 0;
          if (getWavPayloadRange(audioBuffer, audioLength, &payloadOffset, &payloadLength))
          {
            payloadLength &= ~static_cast<size_t>(0x01);
            Serial.printf("[语音合成] 音频下载完成，总长度=%u，PCM偏移=%u，PCM长度=%u\n",
                          static_cast<unsigned>(audioLength),
                          static_cast<unsigned>(payloadOffset),
                          static_cast<unsigned>(payloadLength));
            clearAudio();
            playAudio(audioBuffer + payloadOffset, payloadLength);
            delay(200);
            clearAudio();
          }
          else
          {
            Serial.println("[语音合成] 错误: 无法解析WAV音频数据");
          }
          free(audioBuffer);
        }
      }
      else if (contentType.equals("application/json"))
      {
        Serial.println("合成出现错误");
        String response = http.getString(); // Print the error JSON for debugging
        Serial.println(response);
      }
      else
      {
        Serial.println("未知的Content-Type: " + contentType);
      }
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      String response = http.getString(); // Print the error response for debugging
      Serial.println(response);
    }
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

