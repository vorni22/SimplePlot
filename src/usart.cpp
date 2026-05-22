#include "usart.h"
#include <avr/io.h>
#include <stdio.h>

static int usart_putchar_stdio(char c, FILE* stream);
static FILE usart_stdout;

void usart_init(uint32_t baud) {
    const uint16_t ubrr = (F_CPU / 16 / baud) - 1;

    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFF);

    UCSR0A = 0;
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8N1

    fdev_setup_stream(&usart_stdout, usart_putchar_stdio, nullptr, _FDEV_SETUP_WRITE);
    stdout = &usart_stdout;
}

int usart_putchar(char c) {
    if (c == '\n') {
        usart_putchar('\r');
    }
    while (!(UCSR0A & (1 << UDRE0))) { }
    UDR0 = (uint8_t)c;
    return 0;
}

static int usart_putchar_stdio(char c, FILE* stream) {
    (void)stream;
    return usart_putchar(c);
}

void usart_write(const char* s) {
    while (*s) {
        usart_putchar(*s++);
    }
}