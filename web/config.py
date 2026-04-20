import os

try:
    from local_config import *  # type: ignore # noqa: F403,F401
except ImportError:
    pass


PLACEHOLDER_MARKERS = ("YOUR_", "CHANGE_ME", "XXXX")


def _read_value(name: str, default: str = "") -> str:
    env_value = os.getenv(name)
    if env_value:
        return env_value
    return str(globals().get(name, default))


def _normalize_dashscope_key(value: str) -> str:
    value = (value or "").strip()
    if value and not value.startswith("sk-"):
        return f"sk-{value}"
    return value


def _is_missing(value: str) -> bool:
    value = (value or "").strip()
    if not value:
        return True

    upper = value.upper()
    return any(marker in upper for marker in PLACEHOLDER_MARKERS)


QWEATHER_API_KEY = _read_value("QWEATHER_API_KEY")
AL_API_KEY = _normalize_dashscope_key(_read_value("AL_API_KEY"))
AMAP_API_KEY = _read_value("AMAP_API_KEY")
AMAP_SECURITY_KEY = _read_value("AMAP_SECURITY_KEY")
QWEATHER_API_HOST = _read_value("QWEATHER_API_HOST")

DEFAULT_LOCATION = _read_value("DEFAULT_LOCATION", "116.407394,39.904211")
DEFAULT_WEATHER_CITY = _read_value("DEFAULT_WEATHER_CITY", "Beijing")

CONFIG_VALUES = {
    "QWEATHER_API_KEY": QWEATHER_API_KEY,
    "QWEATHER_API_HOST": QWEATHER_API_HOST,
    "AL_API_KEY": AL_API_KEY,
    "AMAP_API_KEY": AMAP_API_KEY,
}


def get_missing_keys(*keys):
    keys = keys or tuple(CONFIG_VALUES.keys())
    return [key for key in keys if _is_missing(str(CONFIG_VALUES.get(key, "")))]
