#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

struct GpsFix
{
    double latitude;
    double longitude;
    bool valid;
    unsigned long updatedAt;
    String utcTime;
};

void gpsInit();
void gpsPoll();
bool gpsHasValidFix();
GpsFix gpsGetLatestFix();
void gpsTask(void *pvParameters);

#endif // GPS_H
