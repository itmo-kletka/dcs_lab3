#include "uart.h"
#include "uart_drv.h"
#include "gpio_drv.h"

#define BAUD       38400u
#define TX_PIN     6u
#define RX_PIN     7u
#define USART6_AF  8u
#define IRQ_PRIO   6u

static uint8_t rx_data[UART_RX_CAPACITY];
static uint16_t rx_head;
static uint16_t rx_tail;
static uint16_t rx_count;

static uint8_t tx_data[UART_TX_CAPACITY];
static uint16_t tx_head;
static uint16_t tx_tail;
static uint16_t tx_count;

static void ring_push(uint8_t *data, uint16_t cap, uint16_t *head, uint16_t *count, uint8_t byte)
{
    uint32_t mask = __get_PRIMASK();

    __disable_irq();
    if (*count < cap) {
        data[*head] = byte;
        *head = (uint16_t)((*head + 1u) % cap);
        (*count)++;
    }
    __set_PRIMASK(mask);
}

static int ring_pop(uint8_t *data, uint16_t cap, uint16_t *tail, uint16_t *count, uint8_t *byte)
{
    uint32_t mask;
    int ok = 0;

    mask = __get_PRIMASK();
    __disable_irq();
    if (*count > 0u) {
        *byte = data[*tail];
        *tail = (uint16_t)((*tail + 1u) % cap);
        (*count)--;
        ok = 1;
    }
    __set_PRIMASK(mask);
    return ok;
}

static void tx_kick(void)
{
    if (tx_count > 0u) {
        uart_drv_set_txe_it(USART6, 1);
    }
}

void uart_irq_handler(void)
{
    uint8_t byte;

    while (uart_drv_read_byte(USART6, &byte)) {
        ring_push(rx_data, UART_RX_CAPACITY, &rx_head, &rx_count, byte);
    }

    if (uart_drv_txe_it_enabled(USART6) && uart_drv_tx_ready(USART6)) {
        if (ring_pop(tx_data, UART_TX_CAPACITY, &tx_tail, &tx_count, &byte)) {
            (void)uart_drv_write_byte(USART6, byte);
        } else {
            uart_drv_set_txe_it(USART6, 0);
        }
    }
}

void uart_init(void)
{
    gpio_pin_config_t pin_cfg;
    uart_drv_config_t uart_cfg;

    pin_cfg.mode = GPIO_PIN_MODE_AF;
    pin_cfg.open_drain = 0;
    pin_cfg.speed = 2u;
    pin_cfg.pull = GPIO_PIN_PULL_NONE;
    pin_cfg.alt = USART6_AF;
    gpio_init_pin(GPIOC, TX_PIN, &pin_cfg, 1);

    pin_cfg.pull = GPIO_PIN_PULL_UP;
    gpio_init_pin(GPIOC, RX_PIN, &pin_cfg, 1);

    uart_cfg.baud = BAUD;
    uart_cfg.word_len = UART_DRV_WORDLEN_8B;
    uart_cfg.stop_bits = UART_DRV_STOPBITS_1;
    uart_cfg.parity = UART_DRV_PARITY_NONE;

    uart_drv_init_it(USART6, &uart_cfg, IRQ_PRIO);
}

void uart_poll(void)
{
}

int uart_read(uint8_t *byte)
{
    if (byte == 0) {
        return 0;
    }
    return ring_pop(rx_data, UART_RX_CAPACITY, &rx_tail, &rx_count, byte);
}

void uart_write(const uint8_t *data, unsigned len)
{
    unsigned i;

    if (data == 0) {
        return;
    }
    for (i = 0; i < len; i++) {
        ring_push(tx_data, UART_TX_CAPACITY, &tx_head, &tx_count, data[i]);
    }
    tx_kick();
}

void uart_write_str(const char *s)
{
    if (s == 0) {
        return;
    }
    while (*s) {
        uint8_t byte = (uint8_t)*s;
        ring_push(tx_data, UART_TX_CAPACITY, &tx_head, &tx_count, byte);
        s++;
    }
    tx_kick();
}
