#include <avr/io.h>
#define RED1 PB5
#define GREEN1 PB4
#define RED2 PB3
void led_setup();
void led_set(unsigned char port);
void led_clear(unsigned char port);
void led_toggle(unsigned char port);
