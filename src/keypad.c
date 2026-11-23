#include "keypad.h"
#include "i2c_bus.h"
#include "stm32f4xx_hal.h"

/* В PCA9538 (0xE2): P0..P3 — строки, P4..P6 — столбцы (подтянуты к 1). */
#define DEBOUNCE_MS 30u

static uint8_t stable_key;
static uint8_t last_raw_key;
static uint32_t changed_at;
static uint8_t pressed_event;

static uint8_t scan_matrix(void)
{
    uint8_t row;
    uint8_t found_key = 0;
    uint8_t pressed_count = 0;

    for (row = 0; row < 4u; row++) {
        uint8_t cfg = (uint8_t)~(1u << row);
        uint8_t in = 0xFFu;
        uint8_t col;

        /* Опрашиваемая строка — выход с '0', остальные строки и столбцы — входы */
        if (!i2c_bus_write_reg(PCA9538_KBD_ADDR, PCA9538_REG_OUTPUT, 0x00u)) {
            return 0;
        }
        if (!i2c_bus_write_reg(PCA9538_KBD_ADDR, PCA9538_REG_CONFIG, cfg)) {
            return 0;
        }
        if (!i2c_bus_read_reg(PCA9538_KBD_ADDR, PCA9538_REG_INPUT, &in)) {
            return 0;
        }

        for (col = 0; col < 3u; col++) {
            if (!(in & (1u << (col + 4u)))) {
                pressed_count++;
                found_key = (uint8_t)(row * 3u + col + 1u);
            }
        }
    }

    (void)i2c_bus_write_reg(PCA9538_KBD_ADDR, PCA9538_REG_CONFIG, 0xFFu);

    /* При нажатии более одной кнопки считаем, что ни одна не нажата */
    if (pressed_count != 1u) {
        return 0;
    }
    return found_key;
}

void keypad_init(void)
{
    (void)i2c_bus_write_reg(PCA9538_KBD_ADDR, PCA9538_REG_POL, 0x00u);
    (void)i2c_bus_write_reg(PCA9538_KBD_ADDR, PCA9538_REG_OUTPUT, 0x00u);
    (void)i2c_bus_write_reg(PCA9538_KBD_ADDR, PCA9538_REG_CONFIG, 0xFFu);

    stable_key = 0;
    last_raw_key = 0;
    changed_at = HAL_GetTick();
    pressed_event = 0;
}

void keypad_poll(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t raw = scan_matrix();

    if (raw != last_raw_key) {
        last_raw_key = raw;
        changed_at = now;
    }

    if ((now - changed_at) < DEBOUNCE_MS) {
        return;
    }

    if (raw == stable_key) {
        return;
    }

    stable_key = raw;
    if (stable_key != 0u) {
        pressed_event = stable_key;
    }
}

uint8_t keypad_take_key(void)
{
    uint8_t k = pressed_event;
    pressed_event = 0;
    return k;
}
