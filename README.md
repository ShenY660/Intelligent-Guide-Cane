# Intelligent Guide Cane

这是整理后的可分享版本，已经放到 `克隆文件/Intelligent-Guide-Cane-main` 中，保留了原来的项目命名和目录结构：

- `硬件端/`
- `微信小程序/`
- `web/`
- `esp32-cam-platformio/`
- `esp_cam-example/`
- `test/`

项目里的真实 API Key、局域网地址、微信 AppID、热点账号密码都已经移除，统一改成了 `YOUR_*` 占位符，方便别人一眼找到并替换。

## 1. 先改哪些文件

分享给别人之后，优先修改下面这些文件：

1. `硬件端/src/config.local.example.h`
2. `web/local_config.example.py`
3. `微信小程序/config.local.example.js`
4. `微信小程序/project.config.json`
5. `esp32-cam-platformio/src/app_config.h`

推荐做法：

1. 复制 `硬件端/src/config.local.example.h` 为 `硬件端/src/config.local.h`
2. 复制 `web/local_config.example.py` 为 `web/local_config.py`
3. 复制 `微信小程序/config.local.example.js` 为 `微信小程序/config.local.js`
4. 再把里面所有 `YOUR_*` 替换成自己的真实配置

## 2. 各模块要改什么

### 硬件端

文件：`硬件端/src/config.local.example.h`

需要替换的字段：

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `SERVER_BASE_URL`
- `DEVICE_ID`
- `BAIDU_CLIENT_ID`
- `BAIDU_CLIENT_SECRET`
- `BAIDU_CUID`

说明：

- `SERVER_BASE_URL` 必须和 `web` 服务实际地址一致，例如 `http://YOUR_SERVER_API_HOST:12345`
- `DEVICE_ID` 必须和微信小程序里的 `deviceId` 保持一致，否则小程序看不到这根导盲杖的定位

代码里会用到这个地址的位置：

- `硬件端/src/network.cpp`
- `硬件端/src/main.cpp`

实际调用的后端接口：

- `POST /ai`
- `POST /gps`
- `POST /navigation_update`
- `POST /exit_navigation`

### Web 服务端

文件：`web/local_config.example.py`

需要替换的字段：

- `QWEATHER_API_KEY`
- `QWEATHER_API_HOST`
- `AL_API_KEY`
- `AMAP_API_KEY`
- `AMAP_SECURITY_KEY`
- `DEFAULT_LOCATION`
- `DEFAULT_WEATHER_CITY`

说明：

- `QWEATHER_API_HOST` 是和风天气城市查询接口的 Host
- `AL_API_KEY` 是通义千问 / DashScope Key
- `AMAP_API_KEY` 是高德 Web API Key
- `AMAP_SECURITY_KEY` 如你的高德控制台要求安全密钥，再填写

`web/app.py` 暴露的接口：

- `GET /`
- `POST /ai`
- `POST /gps`
- `GET /gps/latest`
- `GET /weather`
- `POST /navigation_update`
- `POST /exit_navigation`

这些接口分别被谁使用：

- `硬件端` 会调用 `/ai`、`/gps`、`/navigation_update`、`/exit_navigation`
- `微信小程序` 会调用 `/gps/latest`

### 微信小程序

文件：`微信小程序/config.local.example.js`

需要替换的字段：

- `amapKey`
- `serverBaseUrl`
- `deviceId`
- `blindStickRefreshMs`
- `defaultDestination`
- `defaultBlindStick`

说明：

- `serverBaseUrl` 必须和硬件端里的 `SERVER_BASE_URL` 指向同一个 `web` 服务
- `deviceId` 必须和硬件端里的 `DEVICE_ID` 一致
- `defaultDestination` 和 `defaultBlindStick` 是默认地图点位，别人可以按自己项目改

另外还要改：

文件：`微信小程序/project.config.json`

需要替换的字段：

- `appid`

说明：

- 这里已经替换成 `YOUR_WECHAT_APPID`
- 别人导入微信开发者工具前，需要改成自己的小程序 AppID

小程序里实际使用这些配置的位置：

- `微信小程序/pages/index/index.js`
- `微信小程序/pages/video/video.js`

### ESP32-CAM

文件：`esp32-cam-platformio/src/app_config.h`

需要替换的字段：

- `CAMERA_WIFI_SSID`
- `CAMERA_WIFI_PASSWORD`
- `CAMERA_HOSTNAME`

说明：

- 这是 ESP32-CAM 要连接的热点信息
- 推荐让 ESP32-CAM、主控硬件、手机在同一个局域网里，调试最方便

ESP32-CAM 对外提供的地址：

- `GET /`
- `GET /health`
- `GET /capture`
- `GET /stream`

小程序视频页里，用户会手动输入摄像头 IP，然后拼出这些地址：

- `http://<camera-ip>/health`
- `http://<camera-ip>/capture`
- `http://<camera-ip>:81/stream`

## 3. 哪些位置已经替换成占位符

这次已经处理过的私有配置位置：

- `硬件端/src/config.h`
- `硬件端/src/config.local.example.h`
- `web/local_config.example.py`
- `web/config.py`
- `微信小程序/config.js`
- `微信小程序/config.local.example.js`
- `微信小程序/project.config.json`
- `esp32-cam-platformio/src/app_config.h`

## 4. 快速启动顺序

### 启动 Web

```bash
cd web
copy local_config.example.py local_config.py
pip install -r requirements.txt
flask --app app run --debug --host 0.0.0.0 --port 12345
```

### 编译硬件端

```bash
cd 硬件端
copy src\\config.local.example.h src\\config.local.h
pio run
pio run --target upload
```

### 打开微信小程序

```bash
cd 微信小程序
copy config.local.example.js config.local.js
```

然后用微信开发者工具打开 `微信小程序/` 目录。

### 编译 ESP32-CAM

```bash
cd esp32-cam-platformio
pio run
pio run --target upload
```

## 5. 对接关系一定要一致

如果别人要复刻整个项目，下面三组值必须统一：

1. `硬件端/src/config.local.h` 的 `SERVER_BASE_URL`
2. `微信小程序/config.local.js` 的 `serverBaseUrl`
3. `web` 实际启动的服务地址

还有这一组也必须一致：

1. `硬件端/src/config.local.h` 的 `DEVICE_ID`
2. `微信小程序/config.local.js` 的 `deviceId`

## 6. 交付建议

如果你准备把这份项目上传给别人，直接上传当前这个整理后的目录即可。别人拿到后，只需要全局搜索 `YOUR_`，就能找到所有需要替换的私有配置。
