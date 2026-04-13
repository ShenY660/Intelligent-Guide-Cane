import os

try:
    from local_config import *  # type: ignore # noqa: F403,F401
except ImportError:
    pass


def _read_value(name: str, default: str = "") -> str:
    env_value = os.getenv(name)
    if env_value:
        return env_value
    return str(globals().get(name, default))


QWEATHER_API_KEY = _read_value("QWEATHER_API_KEY")
AL_API_KEY = _read_value("AL_API_KEY")
AMAP_API_KEY = _read_value("AMAP_API_KEY")

DEFAULT_LOCATION = _read_value("DEFAULT_LOCATION", "120.07275000000001,30.30828611111111")
DEFAULT_WEATHER_CITY = _read_value("DEFAULT_WEATHER_CITY", "南昌市")

CONFIG_VALUES = {
    "QWEATHER_API_KEY": QWEATHER_API_KEY,
    "AL_API_KEY": AL_API_KEY,
    "AMAP_API_KEY": AMAP_API_KEY,
}


def get_missing_keys(*keys):
    keys = keys or tuple(CONFIG_VALUES.keys())
    return [key for key in keys if not CONFIG_VALUES.get(key)]
