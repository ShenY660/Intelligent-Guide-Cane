from flask import Flask, jsonify, request
import threading
import time
from http import HTTPStatus

import dashscope
import requests
from config import (
    AL_API_KEY,
    AMAP_API_KEY,
    DEFAULT_LOCATION,
    DEFAULT_WEATHER_CITY,
    QWEATHER_API_KEY,
    get_missing_keys,
)

app = Flask(__name__)
app.config["JSON_AS_ASCII"] = False

CONVERSATION_TIMEOUT = 60
NAVIGATION_TIMEOUT = 1800
LOCATION_TIMEOUT = 180
ARRIVAL_DISTANCE_METERS = 20

dashscope.api_key = AL_API_KEY or None

CONFIG_LABELS = {
    "QWEATHER_API_KEY": "和风天气 API Key",
    "AL_API_KEY": "DashScope API Key",
    "AMAP_API_KEY": "高德地图 API Key",
}

conversations = {}
last_interaction = {}
conversation_lock = threading.Lock()

navigation_sessions = {}
navigation_lock = threading.Lock()

latest_locations = {}
location_lock = threading.Lock()

PROMPT = """
你是智能导盲杖服务端的意图助手。
你的职责是识别用户输入属于哪一类：导航、退出导航、天气查询、普通对话。

输出规则：
1. 如果用户要去某个地点、请求带路、询问怎么去某地，输出：导航_目的地
2. 如果用户要求结束、退出、停止或取消导航，输出：退出导航
3. 如果用户在问天气，输出：天气_城市_日期
   - 日期只允许：今日、明日、后日
   - 如果用户没有明确说城市，输出：天气_当前城市_对应日期
   - 如果用户没有明确说日期，默认输出：今日
4. 如果以上都不是，直接输出自然中文回复

重要要求：
- 不要解释分类过程
- 导航、退出导航、天气查询必须严格按指定格式输出
- 普通对话不要加前缀，直接回复
- 普通对话尽量控制在50字以内

示例：
- “带我去西湖” -> 导航_西湖
- “导航到上海虹桥站” -> 导航_上海虹桥站
- “退出导航” -> 退出导航
- “今天天气怎么样” -> 天气_当前城市_今日
- “明天北京天气咋样” -> 天气_北京市_明日
- “你好” -> 你好，请问要导航还是查天气？
""".strip()


@app.route("/", methods=["GET"])
def index():
    return "Hello World!"


def build_config_error(*keys):
    missing = get_missing_keys(*keys)
    if not missing:
        return None

    labels = [CONFIG_LABELS.get(key, key) for key in missing]
    return {
        "error": f"服务端缺少配置：{', '.join(labels)}。请先填写 web/local_config.py 或对应环境变量。",
        "missing_config": missing,
    }


def config_error_response(*keys):
    response = jsonify(build_config_error(*keys))
    response.status_code = 503
    return response


def normalize_weather_date(date_text):
    date_text = (date_text or "").strip()
    if date_text in ("", "今天", "今日", "今天天气", "今天的天气"):
        return "今日"
    if date_text in ("明天", "明日", "明天天气", "明天的天气"):
        return "明日"
    if date_text in ("后天", "后日", "后天天气", "后天的天气"):
        return "后日"
    return "今日"


def get_city_id(city_name):
    if not QWEATHER_API_KEY:
        return None
    try:
        response = requests.get(
            "https://geoapi.qweather.com/v2/city/lookup",
            params={
                "location": city_name,
                "key": QWEATHER_API_KEY,
            },
            timeout=10,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("code") == "200" and data.get("location"):
            return data["location"][0]["id"]
        return None
    except requests.RequestException as exc:
        print(f"[WEATHER] 查询城市ID失败: {exc}")
        return None


def get_weather_forecast(city_id):
    if not QWEATHER_API_KEY:
        return None
    try:
        response = requests.get(
            "https://devapi.qweather.com/v7/weather/3d",
            params={
                "location": city_id,
                "lang": "zh",
                "unit": "m",
                "key": QWEATHER_API_KEY,
            },
            timeout=10,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("code") == "200" and data.get("daily"):
            return data["daily"]
        return None
    except requests.RequestException as exc:
        print(f"[WEATHER] 获取天气预报失败: {exc}")
        return None


def format_weather_response(city_name, date_keyword, forecast_data):
    index_map = {"今日": 0, "明日": 1, "后日": 2}
    label_map = {"今日": "今天", "明日": "明天", "后日": "后天"}

    index = index_map.get(date_keyword, 0)
    if index >= len(forecast_data):
        index = 0

    item = forecast_data[index]
    day_label = label_map.get(date_keyword, "今天")
    return (
        f"{city_name}{day_label}{item.get('textDay', '')}，"
        f"{item.get('tempMin', '')}到{item.get('tempMax', '')}度，"
        f"{item.get('windDirDay', '')}{item.get('windScaleDay', '')}级。"
    )


def reverse_geocode(longitude, latitude):
    if not AMAP_API_KEY:
        return None
    try:
        response = requests.get(
            "https://restapi.amap.com/v3/geocode/regeo",
            params={
                "location": f"{longitude},{latitude}",
                "extensions": "base",
                "roadlevel": 0,
                "key": AMAP_API_KEY,
            },
            timeout=10,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("status") != "1":
            return None

        address_component = data.get("regeocode", {}).get("addressComponent", {})
        city = address_component.get("city")
        if isinstance(city, list):
            city = city[0] if city else ""
        if not city:
            city = address_component.get("province", "")

        return {
            "city": city or DEFAULT_WEATHER_CITY,
            "district": address_component.get("district", ""),
            "formatted_address": data.get("regeocode", {}).get("formatted_address", ""),
        }
    except requests.RequestException as exc:
        print(f"[GPS] 逆地理编码失败: {exc}")
        return None


def update_user_location(user_id, latitude, longitude):
    regeo = reverse_geocode(longitude, latitude)
    with location_lock:
        latest_locations[user_id] = {
            "latitude": latitude,
            "longitude": longitude,
            "city": (regeo or {}).get("city", DEFAULT_WEATHER_CITY),
            "district": (regeo or {}).get("district", ""),
            "formatted_address": (regeo or {}).get("formatted_address", ""),
            "updated_at": time.time(),
        }
        return latest_locations[user_id]


def get_user_location(user_id):
    with location_lock:
        info = latest_locations.get(user_id)
        if not info:
            return None
        if time.time() - info["updated_at"] > LOCATION_TIMEOUT:
            return None
        return dict(info)


def get_user_origin(user_id):
    info = get_user_location(user_id)
    if not info:
        return DEFAULT_LOCATION
    return f"{info['longitude']},{info['latitude']}"


def get_user_city(user_id):
    info = get_user_location(user_id)
    if not info or not info.get("city"):
        return DEFAULT_WEATHER_CITY
    return info["city"]


def get_destination_gps(destination):
    if not AMAP_API_KEY:
        return None
    try:
        response = requests.get(
            "https://restapi.amap.com/v3/geocode/geo",
            params={
                "address": destination,
                "output": "json",
                "key": AMAP_API_KEY,
            },
            timeout=10,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("status") == "1" and data.get("geocodes"):
            return data["geocodes"][0]["location"]
        return None
    except requests.RequestException as exc:
        print(f"[NAV] 获取目的地坐标失败: {exc}")
        return None


def get_walking_navigation(origin, destination_gps):
    if not AMAP_API_KEY:
        return None
    try:
        response = requests.get(
            "https://restapi.amap.com/v5/direction/walking",
            params={
                "isindoor": "0",
                "origin": origin,
                "destination": destination_gps,
                "key": AMAP_API_KEY,
                "show_fields": "navi",
            },
            timeout=10,
        )
        response.raise_for_status()
        data = response.json()
        if not (data.get("status") == "1" and data.get("route") and data["route"].get("paths")):
            return None

        path = data["route"]["paths"][0]
        steps = path.get("steps", [])
        return {
            "distance": int(path.get("distance", 0)),
            "duration": int(path.get("cost", {}).get("duration", 0)),
            "steps": steps,
            "first_instruction": steps[0]["instruction"] if steps else "暂无导航指令",
        }
    except requests.RequestException as exc:
        print(f"[NAV] 获取步行导航失败: {exc}")
        return None


def call_qwen(messages):
    if not AL_API_KEY:
        print("[QWEN] Missing DashScope API key")
        return None
    try:
        response = dashscope.Generation.call(
            model="qwen-turbo",
            messages=messages,
            result_format="message",
        )
        if response.status_code == HTTPStatus.OK:
            return response.output.choices[0].message.content.strip()

        print(
            "[QWEN] Request id: %s, status: %s, code: %s, message: %s"
            % (
                response.request_id,
                response.status_code,
                response.code,
                response.message,
            )
        )
        return None
    except Exception as exc:
        print(f"[QWEN] 调用异常: {exc}")
        return None


def start_navigation_session(user_id, destination, destination_gps):
    with navigation_lock:
        navigation_sessions[user_id] = {
            "destination": destination,
            "destination_gps": destination_gps,
            "start_time": time.time(),
            "last_update": time.time(),
        }


def update_navigation_session(user_id):
    with navigation_lock:
        session = navigation_sessions.get(user_id)
        if not session:
            return None

        if time.time() - session["start_time"] > NAVIGATION_TIMEOUT:
            del navigation_sessions[user_id]
            return {"response": "导航已超时，请重新开始导航。", "navigation_complete": True}

        destination = session["destination"]
        destination_gps = session["destination_gps"]
        session["last_update"] = time.time()

    origin = get_user_origin(user_id)
    navigation_info = get_walking_navigation(origin, destination_gps)
    if not navigation_info:
        return {"response": f"无法更新到{destination}的导航路线。"}

    if navigation_info["distance"] <= ARRIVAL_DISTANCE_METERS:
        with navigation_lock:
            navigation_sessions.pop(user_id, None)
        return {
            "response": f"恭喜您，已到达目的地{destination}，导航结束。",
            "navigation_complete": True,
        }

    return {
        "response": navigation_info["first_instruction"],
        "next_instruction": navigation_info["first_instruction"],
        "remaining_distance": navigation_info["distance"],
        "total_duration": navigation_info["duration"],
        "navigation_complete": False,
    }


def handle_exit_navigation(user_id):
    with navigation_lock:
        session = navigation_sessions.pop(user_id, None)
        if session:
            return jsonify(
                {
                    "response": f"已退出到{session['destination']}的导航",
                    "navigation_exited": True,
                }
            )
        return jsonify({"response": "当前没有进行中的导航"})


def handle_ai_response(user_id, qwen_response):
    if qwen_response.startswith("导航_"):
        if get_missing_keys("AMAP_API_KEY"):
            return config_error_response("AMAP_API_KEY")

        destination = qwen_response.split("导航_", 1)[1].strip()
        destination_gps = get_destination_gps(destination)
        if not destination_gps:
            return jsonify({"response": f"找不到{destination}的坐标信息"})

        origin = get_user_origin(user_id)
        navigation_info = get_walking_navigation(origin, destination_gps)
        if not navigation_info:
            return jsonify({"response": f"无法规划到{destination}的步行路线"})

        start_navigation_session(user_id, destination, destination_gps)
        distance_km = navigation_info["distance"] / 1000
        duration_min = navigation_info["duration"] / 60
        summary_msg = (
            f"路线规划完成，全程{distance_km:.1f}公里，"
            f"大约需要{duration_min:.0f}分钟。{navigation_info['first_instruction']}"
        )
        return jsonify(
            {
                "response": summary_msg,
                "navigation_started": True,
                "destination": destination,
                "total_distance": navigation_info["distance"],
                "total_duration": navigation_info["duration"],
            }
        )

    if qwen_response.startswith("天气_"):
        if get_missing_keys("QWEATHER_API_KEY"):
            return config_error_response("QWEATHER_API_KEY")

        parts = qwen_response.split("_", 2)
        city = parts[1].strip() if len(parts) > 1 else ""
        date_keyword = normalize_weather_date(parts[2] if len(parts) > 2 else "今日")

        if city in ("", "当前城市", "定位城市"):
            city = get_user_city(user_id)

        city_id = get_city_id(city)
        if not city_id:
            return jsonify({"response": f"找不到{city}的城市信息"})

        forecast_data = get_weather_forecast(city_id)
        if not forecast_data:
            return jsonify({"response": f"获取{city}天气失败"})

        return jsonify({"response": format_weather_response(city, date_keyword, forecast_data)})

    if qwen_response == "退出导航":
        return handle_exit_navigation(user_id)

    return jsonify({"response": qwen_response})


@app.route("/gps", methods=["POST"])
def receive_gps_data():
    if not request.is_json:
        return jsonify({"error": "Request must be JSON"}), 400

    data = request.get_json()
    latitude = data.get("latitude")
    longitude = data.get("longitude")

    if latitude is None or longitude is None:
        return jsonify({"error": "Missing latitude or longitude"}), 400

    try:
        latitude = float(latitude)
        longitude = float(longitude)
    except (TypeError, ValueError):
        return jsonify({"error": "Invalid latitude or longitude"}), 400

    user_id = request.remote_addr
    info = update_user_location(user_id, latitude, longitude)
    print(
        f"[GPS] {user_id} lat={latitude:.6f} lon={longitude:.6f} city={info['city']} district={info['district']}"
    )

    return (
        jsonify(
            {
                "message": "GPS data received successfully",
                "city": info["city"],
                "district": info["district"],
                "formatted_address": info["formatted_address"],
            }
        ),
        200,
    )


@app.route("/weather", methods=["GET"])
def read_weather():
    user_id = request.remote_addr
    city = request.args.get("city", "").strip()
    date_keyword = normalize_weather_date(request.args.get("date", "今日"))

    if get_missing_keys("QWEATHER_API_KEY"):
        return config_error_response("QWEATHER_API_KEY")

    if not city:
        city = get_user_city(user_id)

    city_id = get_city_id(city)
    if not city_id:
        return jsonify({"error": "找不到城市信息"}), 404

    forecast_data = get_weather_forecast(city_id)
    if not forecast_data:
        return jsonify({"error": "获取天气预报失败"}), 500

    return jsonify(
        {
            "city": city,
            "date": date_keyword,
            "response": format_weather_response(city, date_keyword, forecast_data),
            "daily": forecast_data,
        }
    )


@app.route("/ai", methods=["POST"])
def chat():
    user_id = request.remote_addr
    user_message = request.json.get("message") if request.is_json else None
    if not user_message:
        return jsonify({"error": "请求中缺少 'message' 字段"}), 400

    if get_missing_keys("AL_API_KEY"):
        return config_error_response("AL_API_KEY")

    with conversation_lock:
        current_time = time.time()
        if user_id in conversations:
            if current_time - last_interaction.get(user_id, 0) > CONVERSATION_TIMEOUT:
                conversations[user_id] = [{"role": "system", "content": PROMPT}]
            last_interaction[user_id] = current_time
        else:
            conversations[user_id] = [{"role": "system", "content": PROMPT}]
            last_interaction[user_id] = current_time

        conversations[user_id].append({"role": "user", "content": user_message})
        qwen_response = call_qwen(conversations[user_id])

        if not qwen_response:
            conversations[user_id].pop()
            return jsonify({"response": "调用通义千问失败，请稍后重试。"}), 200

        conversations[user_id].append({"role": "assistant", "content": qwen_response})
        return handle_ai_response(user_id, qwen_response)


@app.route("/navigation_update", methods=["POST"])
def navigation_update():
    if get_missing_keys("AMAP_API_KEY"):
        return config_error_response("AMAP_API_KEY")

    user_id = request.remote_addr
    result = update_navigation_session(user_id)
    if result:
        return jsonify(result), 200
    return jsonify({"response": "当前没有进行中的导航"}), 404


@app.route("/exit_navigation", methods=["POST"])
def exit_navigation():
    user_id = request.remote_addr
    return handle_exit_navigation(user_id)


if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=12345)
