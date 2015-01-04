#include "esp8266.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

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

void echo_handlers();
void button_setup();
void send_reset();

typedef enum {
    ATE0,
    CWJAP,
    RESET,
    REQ_IP,
    CIPMUX,
    CIPSTART,
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

char *_ssid;
char *_password;
void join_ap()
{
    last_req = CWJAP;
    //AT+CWJAP="ssid","pass";
    char cmdstr[64] = "AT+CWJAP=\"";
    strncat(cmdstr, _ssid, 20);
    strcat(cmdstr, "\",\"");
    strncat(cmdstr, _password, 20);
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

void (*_ready_callback)();
void esp8266_setup(char *ssid, char *password, void (*ready_callback)())
{
    _ssid = ssid;
    _password = password;
    _ready_callback = ready_callback;
    uart_setup();
    sei();
    send_reset();
}

void ready_to_use()
{
    if (_ready_callback) {
        _ready_callback();
        usart_puts("Calling _ready_callback");
    }
    else {
        usart_puts("Callback wasn't set?");
    }
}

void get_wifi_ip()
{
    expect_resp = 1;
    last_req = REQ_IP;
    char cmdstr[] = "AT+CIFSR\r\n";
    usart_puts(cmdstr);
}

void set_ip_mux()
{
    usart_puts("AT+CIPMUX=1\r\n");
    last_req = CIPMUX;
}

void open_connection()
{
    usart_puts("AT+CIPSTART=1,\"TCP\",\"144.76.186.140\",9091\r\n");
    last_req = CIPSTART;
}

void linked()
{
    // Connected
}

void srsly_error()
{
    // CIPSTART failed
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
        wifi_state = INIT;
    }
    else if (last_req == ATE0) {
        join_ap();
        uart_echo = 0;
    }
    else if (last_req == CWJAP) {
        wifi_state = WIFI_UP;
        get_wifi_ip();
    }
    else if (last_req == REQ_IP) {
        set_ip_mux();
    }
    else if (last_req == CIPMUX) {
        ready_to_use();
    }
    else if (last_req == CIPSTART) {
    }
    else {
        usart_puts("Last: ");
        usart_putc(last_req);
    }
}

void got_ip_addr()
{
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
}

typedef struct resp_handler {
    char *key;
    void (*callback)();
} resp_handler_s;

struct resp_handler handlers[5] = {
    { .key = "OK", .callback = &got_ok },
    { .key = "Error", .callback = &error },
    { .key = "Linked", .callback = &linked },
    { .key = "ERROR", .callback = &srsly_error },
    { .key = "ALREAY CONNECT", .callback = 0x00 },
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
                if (!handlers[i].callback) {
                    // Nonsense stuff like "ALREAY CONNECT" are nulled out.
                    return;
                }
                (handlers[i].callback)();
                return;
            }
        }
        if (wifi_state == INIT && uart_in[0] == '[' && strncmp(uart_in + 1, "System Ready", 12) == 0) {
            got_ready();
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
