#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>

/* Драйвер шины I2C1 стенда SDK-1.1M (PB8 SCL, PB9 SDA, 400 кГц).
 * Все адреса передаются в 8-битном виде (с младшим битом R/W = 0), как в пособии:
 *   0x78 — OLED SSD1306
 *   0xE0 — PCA9538 (сброс дисплея и линии прерываний)
 *   0xE2 — PCA9538 (матричная клавиатура 3x4) */

#define PCA9538_SYS_ADDR   0xE0u
#define PCA9538_KBD_ADDR   0xE2u

#define PCA9538_REG_INPUT  0x00u
#define PCA9538_REG_OUTPUT 0x01u
#define PCA9538_REG_POL    0x02u
#define PCA9538_REG_CONFIG 0x03u

void i2c_bus_init(void);

int i2c_bus_ready(uint16_t dev_addr8);
int i2c_bus_transmit(uint16_t dev_addr8, const uint8_t *data, uint16_t len);
int i2c_bus_mem_write(uint16_t dev_addr8, uint8_t reg, const uint8_t *data, uint16_t len);
int i2c_bus_mem_read(uint16_t dev_addr8, uint8_t reg, uint8_t *data, uint16_t len);

int i2c_bus_write_reg(uint16_t dev_addr8, uint8_t reg, uint8_t val);
int i2c_bus_read_reg(uint16_t dev_addr8, uint8_t reg, uint8_t *val);

#endif
