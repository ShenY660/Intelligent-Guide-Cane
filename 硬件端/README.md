# 硬件端固件

`硬件端/` 是智能导盲杖主控固件工程，运行在 `ESP32-S3` 上，使用 PlatformIO + Arduino 框架。

## 主要能力

- 语音唤醒与单轮语音交互
- 百度语音识别与语音播报
- GPS 采集与位置上报
- 超声波避障提醒
- 光敏检测与自动照明
- 与服务端联动的天气查询、导航与普通问答

## 本地配置

先复制配置模板：

```bash
copy src\config.local.example.h src\config.local.h
```

然后填写：

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `SERVER_BASE_URL`
- `BAIDU_CLIENT_ID`
- `BAIDU_CLIENT_SECRET`

## 构建与烧录

```bash
pio run
pio run --target upload
```

如果需要串口监视：

```bash
pio device monitor -b 115200
```

## 关键文件

- `src/main.cpp`：任务调度、语音流程、导航更新
- `src/network.cpp`：服务端接口通信
- `src/voice.cpp`：录音、ASR、TTS
- `src/gps.cpp`：GPS 解析与上传
- `src/config.h`：公共默认配置
- `src/config.local.h`：本地私有配置，不提交

## 接线说明

更简化的接线说明见 [接线说明.md](/D:/Intelligent-Guide-Cane/硬件端/test/接线说明.md)。

完整项目启动顺序以仓库根目录的 [README.md](/D:/Intelligent-Guide-Cane/README.md) 为准。
