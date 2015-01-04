#include <avr/io.h>
#include "debug_leds.h"

void led_set(unsigned char port)
{
    PORTB |= (1 << (port));
}

void led_clear(unsigned char port)
{
    PORTB &= ~(1 << (port));
}

void led_toggle(unsigned char port)
{
    PORTB ^= _BV(port);
}

void led_setup()
{
    DDRB |= _BV(DDB5);
    DDRB |= _BV(DDB4);
    DDRB |= _BV(DDB3);
}