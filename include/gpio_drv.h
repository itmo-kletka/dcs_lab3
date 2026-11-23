#ifndef GPIO_DRV_H
#define GPIO_DRV_H

#include "stm32f4xx.h"
#include <stdint.h>

typedef enum {
    GPIO_PIN_MODE_INPUT = 0,
    GPIO_PIN_MODE_OUTPUT = 1,
    GPIO_PIN_MODE_AF = 2,
    GPIO_PIN_MODE_ANALOG = 3
} gpio_pin_mode_t;

typedef enum {
    GPIO_PIN_PULL_NONE = 0,
    GPIO_PIN_PULL_UP = 1,
    GPIO_PIN_PULL_DOWN = 2
} gpio_pin_pull_t;

typedef struct {
    gpio_pin_mode_t mode;
    uint8_t open_drain;
    uint8_t speed;
    gpio_pin_pull_t pull;
    uint8_t alt;
} gpio_pin_config_t;

void gpio_init_pin(GPIO_TypeDef *port, uint8_t pin,
                   const gpio_pin_config_t *cfg, int out_level);

void gpio_get_pin_config(GPIO_TypeDef *port, uint8_t pin, gpio_pin_config_t *cfg);

void gpio_write_pin(GPIO_TypeDef *port, uint8_t pin, int level);
int gpio_read_pin(GPIO_TypeDef *port, uint8_t pin);

void gpio_write_pins(GPIO_TypeDef *port, uint16_t value, uint16_t mask);
uint16_t gpio_read_pins(GPIO_TypeDef *port, uint16_t mask);

#endif
