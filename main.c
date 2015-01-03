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

void send_reset()
{
    usart_puts("AT+RST\r\n");
}
void echo_handlers();
void button_setup();
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

int debug_mode = 0;

typedef enum {
	NORMAL,
	RESP_IP_ADDR,
} parser_state_t;
parser_state_t parser_state = NORMAL;

void get_wifi_ip()
{
    debug_mode = 1;
    char cmdstr[] = "AT+CIFSR\r\n";
    _delay_ms(30);
    usart_puts(cmdstr);
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
        join_ap();
    }
}

void got_ok()
{
    if (wifi_state == JOINING_AP) {
        wifi_state = WIFI_UP;
        get_wifi_ip();
    }
}

void got_ip_addr()
{
    led_set(RED1);
    usart_puts("Gots me an ip: ");
    usart_puts(uart_in);
}

typedef struct resp_handler {
    char *key;
    void (*callback)();
} resp_handler_s;

struct resp_handler handlers[4] = {
    { .key = "ready", .callback = &got_ready },
    { .key = "OK", .callback = &got_ok },
    { .key = "AT+RST", .callback = &at_rst_echo },
    { .key = "AT+CIFSR", .callback = &at_cifsr_echo },
};

void handle_command()
{
    if (parser_state == RESP_IP_ADDR) {
        got_ip_addr();
        parser_state = NORMAL;
    }
    else {
        unsigned int i;
        for (i = 0; i < 4; i++) {
            if (strncmp(handlers[i].key, uart_in, 32) == 0) {
                (handlers[i].callback)();
                return;
            }
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

void button_setup()
{
    DDRD = 1 << PD2;
    PORTD = 1 << PD2;
    
    EIMSK = 1 << INT0;
    MCUCR = 1 << ISC01 | 1 << ISC00;
}

ISR(INT0_vect)
{
    get_wifi_ip();
}

/*

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

typedef enum {
	NORMAL,
	RESP_MODE,
	AT_RESP_MODE,
	RESP_BODY,
	RESP_BODY_IP_ADDR,
} parser_state_t;
parser_state_t parser_state = NORMAL;

void got_echo()
{
    if (strncmp(uart_in, "AT+RST", 6) == 0) {
    }
    else if (strncmp(uart_in, "AT+CIFSR", 8) == 0) {
        led_set(GREEN1);
        uart_in_pos = 0; // Getting ready to read the addr
        parser_state = RESP_BODY_IP_ADDR;
    }
    else {
        led_set(RED2);
    }
}

ISR (USART_RX_vect)
{
    char c = UDR0;
    //usart_putc(c);

    if (c == NL) {
        parse_command();
        uart_in_pos = 0;
    }
    else if (c == CR) {
        if (parser_state == RESP_BODY_IP_ADDR) {
            led_set(RED2);
            usart_puts("Got ip: ");
            uart_in[uart_in_pos] = 0x00;
            usart_puts(uart_in);
            parser_state = NORMAL;
        }
        if (strncmp(uart_in, "AT+", 3) == 0) {
            // Start of a command being echoed back.
            uart_in[uart_in_pos] = 0x00;
            uart_in_pos++;
            got_echo();
        }
        else {
            led_set(RED1);
        }
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
*/