#include "i2c_bus.h"
#include "gpio_drv.h"
#include "stm32f4xx_hal.h"

#define I2C_SCL_PIN   8u
#define I2C_SDA_PIN   9u
#define I2C1_AF       4u
#define I2C_TIMEOUT   25u

static I2C_HandleTypeDef hi2c1;

static void i2c_bus_unstick(void)
{
    gpio_pin_config_t cfg;
    unsigned i;

    /* Если ведомое устройство удерживало SDA во время сброса МК,
     * выдаём 9 тактов на SCL в режиме open-drain GPIO, затем STOP. */
    cfg.mode = GPIO_PIN_MODE_OUTPUT;
    cfg.open_drain = 1;
    cfg.speed = 3u;
    cfg.pull = GPIO_PIN_PULL_UP;
    cfg.alt = 0;

    gpio_init_pin(GPIOB, I2C_SCL_PIN, &cfg, 1);
    gpio_init_pin(GPIOB, I2C_SDA_PIN, &cfg, 1);

    for (i = 0; i < 9u; i++) {
        volatile unsigned d;
        gpio_write_pin(GPIOB, I2C_SCL_PIN, 0);
        for (d = 0; d < 400u; d++) {
        }
        gpio_write_pin(GPIOB, I2C_SCL_PIN, 1);
        for (d = 0; d < 400u; d++) {
        }
    }
}

void i2c_bus_init(void)
{
    gpio_pin_config_t cfg;

    i2c_bus_unstick();

    __HAL_RCC_I2C1_CLK_ENABLE();

    cfg.mode = GPIO_PIN_MODE_AF;
    cfg.open_drain = 1;
    cfg.speed = 3u;
    cfg.pull = GPIO_PIN_PULL_UP;
    cfg.alt = I2C1_AF;

    gpio_init_pin(GPIOB, I2C_SCL_PIN, &cfg, 1);
    gpio_init_pin(GPIOB, I2C_SDA_PIN, &cfg, 1);

    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000u;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    (void)HAL_I2C_Init(&hi2c1);
    (void)HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
    (void)HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0);
}

int i2c_bus_ready(uint16_t dev_addr8)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, dev_addr8, 2u, I2C_TIMEOUT) == HAL_OK) ? 1 : 0;
}

int i2c_bus_transmit(uint16_t dev_addr8, const uint8_t *data, uint16_t len)
{
    if (HAL_I2C_Master_Transmit(&hi2c1, dev_addr8, (uint8_t *)(uintptr_t)data, len, I2C_TIMEOUT) == HAL_OK) {
        return 1;
    }
    return 0;
}

int i2c_bus_mem_write(uint16_t dev_addr8, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if (HAL_I2C_Mem_Write(&hi2c1, dev_addr8, reg, I2C_MEMADD_SIZE_8BIT,
                          (uint8_t *)(uintptr_t)data, len, I2C_TIMEOUT) == HAL_OK) {
        return 1;
    }
    return 0;
}

int i2c_bus_mem_read(uint16_t dev_addr8, uint8_t reg, uint8_t *data, uint16_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c1, dev_addr8, reg, I2C_MEMADD_SIZE_8BIT,
                         data, len, I2C_TIMEOUT) == HAL_OK) {
        return 1;
    }
    return 0;
}

int i2c_bus_write_reg(uint16_t dev_addr8, uint8_t reg, uint8_t val)
{
    return i2c_bus_mem_write(dev_addr8, reg, &val, 1u);
}

int i2c_bus_read_reg(uint16_t dev_addr8, uint8_t reg, uint8_t *val)
{
    return i2c_bus_mem_read(dev_addr8, reg, val, 1u);
}
