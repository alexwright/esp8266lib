//#include <avr/iom328p.h>
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <avr/interrupt.h>
#include <string.h>

#include "softuart.h"

#include "wifi_settings.h"

//#define F_CPU 16000000UL
#define UART_BAUD 9600
#define BAUD_PRESCALE (((F_CPU / (UART_BAUD * 16UL))) - 1)

#define NL '\n'
#define CR '\r'
#define CRNL "\r\n"

unsigned int uart_in_pos = 0;
unsigned int uart_in_size = 64;
unsigned char uart_in[64];
unsigned int uart_out_pos = 0;
unsigned char uart_out[64];

void uart_setup()
{
    // Turn on USART hardware (RX, TX)
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
    // 8 bit char sizes
    UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01);
    // Set baud rate
    UBRR0H = (BAUD_PRESCALE >> 8);
    UBRR0L = BAUD_PRESCALE;
    //UBRR0 = 0x0033;
    // Enable the USART Receive interrupt
    UCSR0B |= (1 << RXCIE0 );
}

void usart_putc (char send)
{
    // Do nothing for a bit if there is already
    // data waiting in the hardware to be sent
    while ((UCSR0A & (1 << UDRE0)) == 0) {};
    UDR0 = send;
}

void usart_puts (const char *send)
{
    // Cycle through each character individually
    while (*send) {
        usart_putc(*send++);
    }
}

void debug(char *str)
{
	softuart_puts(str);
}

void led_toggle()
{
    PORTB ^= _BV(PB5);
}

void send_reset()
{
    usart_puts("AT+RST\r\n");
}

int main (void)
{
    softuart_init();
    DDRB |= _BV(DDB5);
    uart_setup();
    sei();
    send_reset();
    debug("Boot\r\n");
    while (1) {
    }
}

typedef enum {
    INIT,
    READY,
    JOINING_AP,
    WIFI_UP,
    CONNECTING_TCP,
} wifi_state_t;
wifi_state_t wifi_state = INIT;

void join_ap()
{
    wifi_state = JOINING_AP;
    //AT+CWJAP="ssid","pass";
    char cmdstr[64] = "AT+CWJAP=\"";
    strncat(cmdstr, WIFI_SSID, 20);
    strcat(cmdstr, "\",\"");
    strncat(cmdstr, WIFI_WPAPSK, 20);
    strcat(cmdstr, "\";\r\n");
    usart_puts(cmdstr);
}

void get_wifi_ip()
{
    char cmdstr[] = "AT+CIFSR\r\n";
    usart_puts(cmdstr);
}

void got_ok()
{
    if (wifi_state == JOINING_AP) {
        wifi_state = WIFI_UP;
        get_wifi_ip();
        led_toggle();
    }
}

void got_ready()
{
    if (wifi_state == INIT) {
        led_toggle();
        wifi_state = READY;
        join_ap();
    }
}

void parse_command()
{
    if (strncmp(uart_in, "ready", 5) == 0) {
        got_ready();
    }
    else if (strncmp(uart_in, "OK", 2) == 0) {
        got_ok();
    }
    else {
    }
}

ISR (USART_RX_vect)
{
    char c = UDR0;

    if (c == NL) {
        uart_in_pos = 0;
        parse_command();
    }
    else if (c == CR) {
        // Just ignore CR for now
    }
    else {
        uart_in[uart_in_pos] = c;
        uart_in_pos++;
    }

    if (uart_in_pos == uart_in_size) {
        // About to overrun and go up in flames!
        uart_in_pos = 0;
    }
}
