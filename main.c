#include "esp8266.h"
#include "wifi_settings.h"
#include "debug_leds.h"

void wifi_is_up()
{
    led_toggle(GREEN1);
}

int main (void)
{
    led_setup();
    esp8266_setup(WIFI_SSID, WIFI_WPAPSK, &wifi_is_up);
    led_set(RED1);
    led_set(GREEN1);
    while (1) {
    }
}