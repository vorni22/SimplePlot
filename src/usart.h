#ifndef USART_H
#define USART_H

#include <stdint.h>

void usart_init(uint32_t baud);
int usart_putchar(char c);
void usart_write(const char* s);

#endif