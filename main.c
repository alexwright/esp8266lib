//#include <avr/iom328p.h>
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/delay.h>

#include "wifi_settings.h"

//#define F_CPU 16000000UL
#define UART_BAUD 57600
#define BAUD_PRESCALE (((F_CPU / (UART_BAUD * 16UL))) - 1)

#define NL '\n'
#define CR 0x0D
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

#define RED1 PB5
#define GREEN1 PB4
#define RED2 PB3

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

void echo_handlers();
void button_setup();
void send_reset();

int main (void)
{
    //softuart_init();
    DDRB |= _BV(DDB5);
    DDRB |= _BV(DDB4);
    DDRB |= _BV(DDB3);
    uart_setup();
    sei();
    send_reset();
    button_setup();
    while (1) {
    }
}

typedef enum {
    ATE0,
    CWJAP,
    RESET,
    REQ_IP,
    CIPMUX,
    NONE,
} last_req_t;
last_req_t last_req = NONE;

typedef enum {
    UNKNOWN,
    INIT,
    READY,
    WIFI_UP,
} wifi_state_t;
wifi_state_t wifi_state = UNKNOWN;

void send_reset()
{
    last_req = RESET;
    usart_puts("AT+RST\r\n");
}

void disable_echo()
{
    last_req = ATE0;
    usart_puts("ATE0\r\n");
}

void join_ap()
{
    last_req = CWJAP;
    //AT+CWJAP="ssid","pass";
    char cmdstr[64] = "AT+CWJAP=\"";
    strncat(cmdstr, WIFI_SSID, 20);
    strcat(cmdstr, "\",\"");
    strncat(cmdstr, WIFI_WPAPSK, 20);
    strcat(cmdstr, "\";\r\n");
    usart_puts(cmdstr);
}

int debug_mode = 0;
int uart_echo = 1;
int expect_resp = 0;

typedef enum {
	NORMAL,
	RESP_IP_ADDR,
} parser_state_t;
parser_state_t parser_state = NORMAL;

void get_wifi_ip()
{
    expect_resp = 1;
    last_req = REQ_IP;
    debug_mode = 1;
    char cmdstr[] = "AT+CIFSR\r\n";
    _delay_ms(30);
    usart_puts(cmdstr);
}

void set_ip_mux()
{
    usart_puts("AT+CIPMUX=1\r\n");
    last_req = CIPMUX;
}

void at_rst_echo()
{
    led_set(RED2);
    //usart_puts("1\r\n");
    wifi_state = INIT; // If we see RST echoed back the module is cycling
}

void at_cifsr_echo()
{
    led_set(GREEN1);
    //usart_puts("2\r\n");
    parser_state = RESP_IP_ADDR;
}

void got_ready()
{
    if (wifi_state == INIT) {
        wifi_state = READY;
        disable_echo();
    }
}

void got_ok()
{
    if (last_req == RESET) {
        //usart_puts("Last: reset\r\n");
        wifi_state = INIT;
    }
    else if (last_req == ATE0) {
        //usart_puts("Last: ATE0\r\n");
        join_ap();
        led_set(RED2);
        uart_echo = 0;
    }
    else if (last_req == CWJAP) {
        //usart_puts("Last: CWJAP\r\n");
        wifi_state = WIFI_UP;
        get_wifi_ip();
    }
    else if (last_req == REQ_IP) {
        set_ip_mux();
    }
    else if (last_req == CIPMUX) {
        led_set(GREEN1);
    }
    else {
        usart_puts("Last: ");
        usart_putc(last_req);
    }
}

void got_ip_addr()
{
    led_set(RED1);
    usart_puts("Gots me an ip: ");
    usart_puts(uart_in);
    usart_puts("\r\n");
}

void got_data()
{
    if (last_req == REQ_IP) {
        expect_resp = 0;
        got_ip_addr();
    }
}

void error()
{
    led_toggle(RED1);
}

typedef struct resp_handler {
    char *key;
    void (*callback)();
} resp_handler_s;

struct resp_handler handlers[5] = {
    { .key = "ready", .callback = &got_ready },
    { .key = "OK", .callback = &got_ok },
    { .key = "AT+RST", .callback = &at_rst_echo },
    { .key = "AT+CIFSR", .callback = &at_cifsr_echo },
    { .key = "Error", .callback = &error },
};

void handle_command()
{
    if (parser_state == RESP_IP_ADDR) {
        got_ip_addr();
        parser_state = NORMAL;
    }
    else {
        unsigned int i;
        for (i = 0; i < 5; i++) {
            if (strncmp(handlers[i].key, uart_in, 32) == 0) {
                (handlers[i].callback)();
                return;
            }
        }
        if (wifi_state == INIT && uart_in[0] == '[' && strncmp(uart_in + 1, "System Ready", 12) == 0) {
            got_ready();
            led_set(RED1);
            return;
        }
        if (expect_resp) {
            got_data();
            return;
        }
        if (wifi_state == INIT || wifi_state == UNKNOWN) {
            // ESP8266 spews a lot of garbage (data, but the wrong baud I think)on boot.
            return;
        }
        if (uart_echo) {
            // Best just to ignore the echo until it's disabled I guess.
            return;
        }
        if (debug_mode) {
            usart_puts("uknown: ");
            usart_puts(uart_in);
        }
    }
}

ISR (USART_RX_vect)
{
    char c = UDR0;
    if (c == 0x0D) {
        uart_in[uart_in_pos] = 0x00;
        uart_in_pos = 0;
        if (strlen(uart_in) == 0) {
            //usart_puts("no input?\r\n");
        }
        else {
            handle_command();
        }
    }
    else if (c == 0x0A) {
        // ignore?
    }
    else {
        uart_in[uart_in_pos++] = c;
    }
}
