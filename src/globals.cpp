#include "config.h"

TaskHandle_t ultrasonicTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t lightSensorTaskHandle = NULL;
TaskHandle_t voiceTaskHandle = NULL;
TaskHandle_t gpsTaskHandle = NULL;

bool isConnectedToWifi = false;
bool record_status_me = true;
String accessToken = "";
