#include "esp8266.h"
#include "wifi_settings.h"

int main (void)
{
    esp8266_setup(WIFI_SSID, WIFI_WPAPSK, 0x00);
    while (1) {
    }
}