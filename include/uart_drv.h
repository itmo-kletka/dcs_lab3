#ifndef UART_DRV_H
#define UART_DRV_H

#include "stm32f4xx.h"
#include <stdint.h>

typedef enum {
    UART_DRV_WORDLEN_8B = 0,
    UART_DRV_WORDLEN_9B = 1
} uart_wordlen_t;

typedef enum {
    UART_DRV_STOPBITS_1 = 0,
    UART_DRV_STOPBITS_2 = 1
} uart_stopbits_t;

typedef enum {
    UART_DRV_PARITY_NONE = 0,
    UART_DRV_PARITY_EVEN = 1,
    UART_DRV_PARITY_ODD  = 2
} uart_parity_t;

typedef struct {
    uint32_t baud;
    uart_wordlen_t word_len;
    uart_stopbits_t stop_bits;
    uart_parity_t parity;
} uart_drv_config_t;

#define UART_DRV_STAT_PE    (1u << 0)
#define UART_DRV_STAT_FE    (1u << 1)
#define UART_DRV_STAT_NE    (1u << 2)
#define UART_DRV_STAT_ORE   (1u << 3)
#define UART_DRV_STAT_RXNE  (1u << 5)
#define UART_DRV_STAT_TC    (1u << 6)
#define UART_DRV_STAT_TXE   (1u << 7)

void uart_drv_init_poll(USART_TypeDef *usart, const uart_drv_config_t *cfg);
void uart_drv_init_it(USART_TypeDef *usart, const uart_drv_config_t *cfg, uint32_t irq_prio);

void uart_drv_set_it_mode(USART_TypeDef *usart, int on, uint32_t irq_prio);
void uart_drv_set_txe_it(USART_TypeDef *usart, int on);
int uart_drv_txe_it_enabled(USART_TypeDef *usart);

uint32_t uart_drv_get_status(USART_TypeDef *usart);
int uart_drv_rx_ready(USART_TypeDef *usart);
int uart_drv_tx_ready(USART_TypeDef *usart);
int uart_drv_tx_complete(USART_TypeDef *usart);

int uart_drv_read_byte(USART_TypeDef *usart, uint8_t *byte);
int uart_drv_write_byte(USART_TypeDef *usart, uint8_t byte);

#endif
