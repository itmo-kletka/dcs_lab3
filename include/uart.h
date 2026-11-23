#ifndef UART_H
#define UART_H

#include <stdint.h>

#ifndef UART_RX_CAPACITY
#define UART_RX_CAPACITY 128
#endif
#ifndef UART_TX_CAPACITY
#define UART_TX_CAPACITY 256
#endif

void uart_init(void);
void uart_poll(void);

int uart_read(uint8_t *byte);
void uart_write(const uint8_t *data, unsigned len);
void uart_write_str(const char *s);

void uart_irq_handler(void);

#endif
