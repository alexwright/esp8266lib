#include "esp8266.h"
#include "wifi_settings.h"
void wifi_is_up()
{
}

int main (void)
{
    esp8266_setup(WIFI_SSID, WIFI_WPAPSK, &wifi_is_up);
    while (1) {
    }
}