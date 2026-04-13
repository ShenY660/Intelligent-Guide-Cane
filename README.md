# Intelligent Guide Cane

基于 `ESP32-S3` 的智能导盲杖项目，当前仓库包含主控固件、Python 服务端、微信小程序，以及 ESP32-CAM 相关辅助工程。

## 仓库结构

- `硬件端/`：主控固件，负责语音交互、GPS 上报、避障、光敏照明等
- `web/`：Flask 服务端，负责意图识别、天气查询、导航规划与位置缓存
- `微信小程序/`：地图与摄像头预览小程序
- `esp32-cam-platformio/`：ESP32-CAM 的 PlatformIO 工程
- `esp_cam-example/`：ESP32-CAM Arduino 示例代码备份
- `test/`：麦克风与扬声器链路测试固件
- `当前已实现功能介绍.md`：当前系统能力说明

## 已实现能力

- 语音唤醒、录音、百度语音识别与语音播报
- GPS 坐标采集、上报、城市反解
- 步行导航与导航状态更新
- 天气查询
- 超声波避障提醒
- 光敏检测与照明控制
- 微信小程序地图检索、路径展示、ESP32-CAM 画面预览

## 安全说明

仓库中的真实 Wi-Fi、API Key、Client Secret 已经移除。

首次运行前，请分别复制并填写以下本地私有配置文件：

- `硬件端/src/config.local.example.h` -> `硬件端/src/config.local.h`
- `web/local_config.example.py` -> `web/local_config.py`
- `微信小程序/config.local.example.js` -> `微信小程序/config.local.js`

这些本地私有文件已经加入 `.gitignore`，不要再次提交到 GitHub。

## 快速开始

### 1. 启动服务端

服务端基于 Flask。按官方推荐方式，可以直接用 `flask --app ... run` 启动开发服务器。

```bash
cd web
pip install -r requirements.txt
copy local_config.example.py local_config.py
flask --app app run --debug --host 0.0.0.0 --port 12345
```

`web/local_config.py` 至少需要填写：

- `QWEATHER_API_KEY`
- `AL_API_KEY`
- `AMAP_API_KEY`

如果不填，服务可以启动，但天气、导航、AI 对话接口会返回缺少配置的提示。

### 2. 编译并烧录主控固件

主控固件使用 PlatformIO。按 PlatformIO CLI 的常规流程，先构建，再上传：

```bash
cd 硬件端
copy src\config.local.example.h src\config.local.h
pio run
pio run --target upload
```

`硬件端/src/config.local.h` 需要填写：

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `SERVER_BASE_URL`
- `BAIDU_CLIENT_ID`
- `BAIDU_CLIENT_SECRET`

其中 `SERVER_BASE_URL` 应指向 ESP32 能访问到的服务端地址，例如：

```c
#define SERVER_BASE_URL "http://192.168.1.100:12345"
```

### 3. 配置微信小程序

```bash
cd 微信小程序
copy config.local.example.js config.local.js
```

`config.local.js` 中至少需要填写 `amapKey`。然后用微信开发者工具打开 `微信小程序/` 目录即可。

视频页会要求手动填写 `ESP32-CAM` 的局域网 IP，用于拼接：

- `http://<camera-ip>:81/stream`
- `http://<camera-ip>/capture`
- `http://<camera-ip>/health`

### 4. 使用 ESP32-CAM 辅助工程

- `esp32-cam-platformio/`：推荐使用的 PlatformIO 版摄像头工程
- `esp_cam-example/`：保留的 Arduino 示例代码，便于对照调试

`esp32-cam-platformio/src/app_config.h` 已经是占位配置，可直接替换成你自己的热点名称和密码。

### 5. 音频链路单独测试

`test/` 目录是一个独立的麦克风/扬声器回放测试工程，可用于硬件联调：

```bash
cd test
pio run
pio run --target upload
```

## 关键接口

主控固件会访问以下服务端接口：

- `POST /ai`
- `POST /gps`
- `POST /navigation_update`
- `POST /exit_navigation`
- `GET /weather`

默认服务端入口文件为 [app.py](/D:/Intelligent-Guide-Cane/web/app.py)。

## 关键文件

- [硬件端/src/main.cpp](/D:/Intelligent-Guide-Cane/硬件端/src/main.cpp)
- [硬件端/src/network.cpp](/D:/Intelligent-Guide-Cane/硬件端/src/network.cpp)
- [硬件端/src/config.h](/D:/Intelligent-Guide-Cane/硬件端/src/config.h)
- [web/app.py](/D:/Intelligent-Guide-Cane/web/app.py)
- [微信小程序/pages/index/index.js](/D:/Intelligent-Guide-Cane/微信小程序/pages/index/index.js)
- [微信小程序/pages/video/video.js](/D:/Intelligent-Guide-Cane/微信小程序/pages/video/video.js)

## 补充说明

- `硬件端/README.md` 提供主控固件的单独说明
- `当前已实现功能介绍.md` 适合答辩、汇报或阶段总结时快速查看
- 服务端和固件现在都支持“仓库模板 + 本地私有配置”的方式发布与开发
