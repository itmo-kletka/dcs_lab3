#include "gpio_drv.h"

static void gpio_clock_on(GPIO_TypeDef *port)
{
    uint32_t index = ((uint32_t)port - (uint32_t)GPIOA) / 0x400u;

    if (index > 8u) {
        return;
    }

    RCC->AHB1ENR |= (1u << index);
    (void)RCC->AHB1ENR;
}

static void set_bits2(volatile uint32_t *reg, uint8_t pin, uint32_t value)
{
    uint32_t shift = (uint32_t)pin * 2u;
    uint32_t tmp = *reg;

    tmp &= ~(3u << shift);
    tmp |= (value & 3u) << shift;
    *reg = tmp;
}

void gpio_init_pin(GPIO_TypeDef *port, uint8_t pin,
                   const gpio_pin_config_t *cfg, int out_level)
{
    if (pin > 15u || cfg == 0) {
        return;
    }

    gpio_clock_on(port);

    if (cfg->mode == GPIO_PIN_MODE_OUTPUT) {
        gpio_write_pin(port, pin, out_level);
    }

    if (cfg->open_drain) {
        port->OTYPER |= (1u << pin);
    } else {
        port->OTYPER &= ~(1u << pin);
    }

    set_bits2(&port->OSPEEDR, pin, cfg->speed);
    set_bits2(&port->PUPDR, pin, (uint32_t)cfg->pull);

    if (pin < 8u) {
        uint32_t shift = (uint32_t)pin * 4u;
        port->AFR[0] = (port->AFR[0] & ~(0xFu << shift)) | ((uint32_t)(cfg->alt & 0xFu) << shift);
    } else {
        uint32_t shift = ((uint32_t)pin - 8u) * 4u;
        port->AFR[1] = (port->AFR[1] & ~(0xFu << shift)) | ((uint32_t)(cfg->alt & 0xFu) << shift);
    }

    set_bits2(&port->MODER, pin, (uint32_t)cfg->mode);
}

void gpio_get_pin_config(GPIO_TypeDef *port, uint8_t pin, gpio_pin_config_t *cfg)
{
    uint32_t shift2;

    if (pin > 15u || cfg == 0) {
        return;
    }

    shift2 = (uint32_t)pin * 2u;
    cfg->mode = (gpio_pin_mode_t)((port->MODER >> shift2) & 3u);
    cfg->open_drain = (uint8_t)((port->OTYPER >> pin) & 1u);
    cfg->speed = (uint8_t)((port->OSPEEDR >> shift2) & 3u);
    cfg->pull = (gpio_pin_pull_t)((port->PUPDR >> shift2) & 3u);

    if (pin < 8u) {
        cfg->alt = (uint8_t)((port->AFR[0] >> (pin * 4u)) & 0xFu);
    } else {
        cfg->alt = (uint8_t)((port->AFR[1] >> ((pin - 8u) * 4u)) & 0xFu);
    }
}

void gpio_write_pin(GPIO_TypeDef *port, uint8_t pin, int level)
{
    if (pin > 15u) {
        return;
    }

    if (level) {
        port->BSRR = (1u << pin);
    } else {
        port->BSRR = (1u << (pin + 16u));
    }
}

int gpio_read_pin(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin > 15u) {
        return 0;
    }

    return (port->IDR & (1u << pin)) ? 1 : 0;
}

void gpio_write_pins(GPIO_TypeDef *port, uint16_t value, uint16_t mask)
{
    uint32_t bsrr;

    bsrr = (uint32_t)(value & mask);
    bsrr |= (uint32_t)((~value) & mask) << 16;
    port->BSRR = bsrr;
}

uint16_t gpio_read_pins(GPIO_TypeDef *port, uint16_t mask)
{
    return (uint16_t)(port->IDR & mask);
}
