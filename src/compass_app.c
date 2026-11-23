#include "compass_app.h"
#include "mag.h"
#include "oled.h"
#include "keypad.h"
#include "uart.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>

#define SENSOR_PERIOD_MS    50u
#define TELEMETRY_PERIOD_MS 200u

#define COMPASS_CX 100
#define COMPASS_CY 35
#define COMPASS_R  22

static uint32_t last_update_ms;
static uint32_t last_telemetry_ms;

static void render_oled(const mag_state_t *st)
{
    char line[24];

    oled_fill(0);

    oled_write_str_at(0, 0, st->sensor_ok ? "COMPASS LSM9DS" : "COMPASS NO-MAG", 1);

    snprintf(line, sizeof(line), "X:%5ld mG", (long)st->x_mgauss);
    oled_write_str_at(0, 12, line, 1);

    snprintf(line, sizeof(line), "Y:%5ld mG", (long)st->y_mgauss);
    oled_write_str_at(0, 22, line, 1);

    snprintf(line, sizeof(line), "Z:%5ld mG", (long)st->z_mgauss);
    oled_write_str_at(0, 32, line, 1);

    snprintf(line, sizeof(line), "Az:%3u deg", (unsigned)st->heading_deg);
    oled_write_str_at(0, 44, line, 1);

    snprintf(line, sizeof(line), "%u/%u/%u%%",
             (unsigned)st->scale_x,
             (unsigned)st->scale_y,
             (unsigned)st->scale_z);
    oled_write_str_at(0, 55, line, 1);

    /* Круговой индикатор со стрелкой-вектором на север в правой части экрана */
    oled_draw_circle(COMPASS_CX, COMPASS_CY, COMPASS_R, 1);
    oled_write_str_at(COMPASS_CX - 2, COMPASS_CY - COMPASS_R - 8, "N", 1);
    oled_draw_arrow(COMPASS_CX, COMPASS_CY,
                    COMPASS_CX + st->vec_x,
                    COMPASS_CY + st->vec_y, 1);

    oled_update();
}

static uint16_t clamp_step(uint16_t val, int delta)
{
    int v = (int)val + delta;
    if (v < (int)MAG_SCALE_MIN) {
        v = (int)MAG_SCALE_MIN;
    }
    if (v > (int)MAG_SCALE_MAX) {
        v = (int)MAG_SCALE_MAX;
    }
    return (uint16_t)v;
}

static void handle_keypad(void)
{
    uint8_t key = keypad_take_key();
    mag_state_t st;

    if (key == 0u) {
        return;
    }

    mag_get_state(&st);
    switch (key) {
    case 1:
        st.scale_x = clamp_step(st.scale_x, 10);
        break;
    case 4:
        st.scale_x = clamp_step(st.scale_x, -10);
        break;
    case 2:
        st.scale_y = clamp_step(st.scale_y, 10);
        break;
    case 5:
        st.scale_y = clamp_step(st.scale_y, -10);
        break;
    case 3:
        st.scale_z = clamp_step(st.scale_z, 10);
        break;
    case 6:
        st.scale_z = clamp_step(st.scale_z, -10);
        break;
    case 11:
        st.scale_x = 100u;
        st.scale_y = 100u;
        st.scale_z = 100u;
        break;
    case 12:
        mag_calibrate_zero();
        break;
    default:
        break;
    }
    (void)mag_set_scales(st.scale_x, st.scale_y, st.scale_z);
}

void compass_app_send_telemetry(void)
{
    char buf[112];
    mag_state_t st;

    mag_get_state(&st);
    snprintf(buf, sizeof(buf),
             "DATA X=%ld Y=%ld Z=%ld SX=%u SY=%u SZ=%u DEG=%u VX=%d VY=%d OK=%u\r\n",
             (long)st.x_mgauss,
             (long)st.y_mgauss,
             (long)st.z_mgauss,
             (unsigned)st.scale_x,
             (unsigned)st.scale_y,
             (unsigned)st.scale_z,
             (unsigned)st.heading_deg,
             (int)st.vec_x,
             (int)st.vec_y,
             (unsigned)st.sensor_ok);
    uart_write_str(buf);
}

void compass_app_init(void)
{
    mag_state_t st;

    /* Сначала аппаратный сброс и инициализация OLED через PCA9538 (0xE0),
     * затем инициализация магнитометра и клавиатуры на той же шине I2C1. */
    oled_init();
    mag_init();
    keypad_init();

    mag_poll();
    mag_get_state(&st);
    render_oled(&st);

    last_update_ms = HAL_GetTick();
    last_telemetry_ms = last_update_ms;
}

void compass_app_poll(void)
{
    uint32_t now = HAL_GetTick();

    keypad_poll();
    handle_keypad();

    if ((now - last_update_ms) >= SENSOR_PERIOD_MS) {
        mag_state_t st;

        last_update_ms = now;
        mag_poll();
        mag_get_state(&st);
        render_oled(&st);
    }

    if ((now - last_telemetry_ms) >= TELEMETRY_PERIOD_MS) {
        last_telemetry_ms = now;
        compass_app_send_telemetry();
    }
}
