#include "mag.h"
#include "i2c_bus.h"

#include <math.h>

#define WHO_AM_I_REG        0x0Fu
#define LSM9DS1_MAG_WHOAMI  0x3Du
#define LSM9DS0_XM_WHOAMI   0x49u

#define LSM303AGR_WHO_REG   0x4Fu
#define LSM303AGR_MAG_WHO   0x40u

/* Регистры магнитометра LSM9DS1 */
#define LSM9DS1_CTRL_REG1_M 0x20u
#define LSM9DS1_CTRL_REG2_M 0x21u
#define LSM9DS1_CTRL_REG3_M 0x22u
#define LSM9DS1_CTRL_REG4_M 0x23u
#define LSM9DS1_CTRL_REG5_M 0x24u
#define LSM9DS1_OUT_X_L_M   0x28u

/* Регистры магнитометра LSM9DS0 */
#define LSM9DS0_CTRL_REG5_XM 0x24u
#define LSM9DS0_CTRL_REG6_XM 0x25u
#define LSM9DS0_CTRL_REG7_XM 0x26u
#define LSM9DS0_OUT_X_L_M    0x08u

/* Регистры магнитометра LSM303AGR / LIS2MDL */
#define LIS2MDL_CFG_REG_A    0x60u
#define LIS2MDL_CFG_REG_C    0x62u
#define LIS2MDL_OUTX_L_REG   0x68u

/* Регистры магнитометра LSM303DLHC */
#define LSM303DLHC_CRA_REG_M 0x00u
#define LSM303DLHC_CRB_REG_M 0x01u
#define LSM303DLHC_MR_REG_M  0x02u
#define LSM303DLHC_OUT_X_H_M 0x03u

typedef enum {
    CHIP_NONE = 0,
    CHIP_LSM9DS1,
    CHIP_LSM9DS0,
    CHIP_LIS2MDL,
    CHIP_LSM303DLHC
} mag_chip_t;

static uint16_t mag_addr;
static uint8_t out_reg;
static mag_chip_t chip_type;
static mag_state_t state;

/* Компенсация собственного магнитного поля платы стенда (динамик BA1 и элементы) */
static int32_t last_raw_x;
static int32_t last_raw_y;
static int32_t last_raw_z;

static int32_t min_x, max_x;
static int32_t min_y, max_y;
static int32_t min_z, max_z;
static int32_t off_x = 225;
static int32_t off_y = 220;
static int32_t off_z = 20;
static uint8_t bounds_inited;

static void probe_and_init_chip(void)
{
    static const uint16_t candidates[] = {0x3Cu, 0x38u, 0x3Au, 0x3Eu};
    unsigned i;

    mag_addr = 0;
    chip_type = CHIP_NONE;

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        uint16_t addr = candidates[i];
        uint8_t who = 0;

        if (!i2c_bus_ready(addr)) {
            continue;
        }

        if (i2c_bus_read_reg(addr, WHO_AM_I_REG, &who)) {
            if (who == LSM9DS1_MAG_WHOAMI) {
                mag_addr = addr;
                chip_type = CHIP_LSM9DS1;
                out_reg = (uint8_t)(LSM9DS1_OUT_X_L_M | 0x80u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG1_M, 0xFCu);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG2_M, 0x00u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG3_M, 0x00u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG4_M, 0x0Cu);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG5_M, 0x40u);
                state.sensor_ok = 1;
                return;
            }
            if (who == LSM9DS0_XM_WHOAMI) {
                mag_addr = addr;
                chip_type = CHIP_LSM9DS0;
                out_reg = (uint8_t)(LSM9DS0_OUT_X_L_M | 0x80u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS0_CTRL_REG5_XM, 0x70u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS0_CTRL_REG6_XM, 0x00u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS0_CTRL_REG7_XM, 0x00u);
                state.sensor_ok = 1;
                return;
            }
        }

        if (i2c_bus_read_reg(addr, LSM303AGR_WHO_REG, &who) && who == LSM303AGR_MAG_WHO) {
            mag_addr = addr;
            chip_type = CHIP_LIS2MDL;
            out_reg = (uint8_t)(LIS2MDL_OUTX_L_REG | 0x80u);
            (void)i2c_bus_write_reg(mag_addr, LIS2MDL_CFG_REG_A, 0x8Cu);
            (void)i2c_bus_write_reg(mag_addr, LIS2MDL_CFG_REG_C, 0x10u);
            state.sensor_ok = 1;
            return;
        }

        if (addr == 0x38u || addr == 0x3Cu) {
            uint8_t cr1 = 0;
            if (i2c_bus_write_reg(addr, LSM9DS1_CTRL_REG1_M, 0xFCu) &&
                i2c_bus_read_reg(addr, LSM9DS1_CTRL_REG1_M, &cr1) && cr1 == 0xFCu) {
                mag_addr = addr;
                chip_type = CHIP_LSM9DS1;
                out_reg = (uint8_t)(LSM9DS1_OUT_X_L_M | 0x80u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG2_M, 0x00u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG3_M, 0x00u);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG4_M, 0x0Cu);
                (void)i2c_bus_write_reg(mag_addr, LSM9DS1_CTRL_REG5_M, 0x40u);
                state.sensor_ok = 1;
                return;
            }
            if (addr == 0x3Cu) {
                mag_addr = addr;
                chip_type = CHIP_LSM303DLHC;
                out_reg = LSM303DLHC_OUT_X_H_M;
                (void)i2c_bus_write_reg(mag_addr, LSM303DLHC_CRA_REG_M, 0x14u);
                (void)i2c_bus_write_reg(mag_addr, LSM303DLHC_CRB_REG_M, 0x20u);
                (void)i2c_bus_write_reg(mag_addr, LSM303DLHC_MR_REG_M, 0x00u);
                state.sensor_ok = 1;
                return;
            }
        }
    }

    state.sensor_ok = 0;
}

void mag_init(void)
{
    state.scale_x = MAG_SCALE_INIT;
    state.scale_y = MAG_SCALE_INIT;
    state.scale_z = MAG_SCALE_INIT;
    state.x_mgauss = 0;
    state.y_mgauss = 0;
    state.z_mgauss = 0;
    state.heading_deg = 0;
    state.vec_x = 0;
    state.vec_y = -18;
    state.sensor_ok = 0;
    bounds_inited = 0;

    probe_and_init_chip();
}

void mag_calibrate_zero(void)
{
    off_x = last_raw_x;
    off_y = last_raw_y;
    off_z = last_raw_z;
    min_x = last_raw_x - 80;
    max_x = last_raw_x + 80;
    min_y = last_raw_y - 80;
    max_y = last_raw_y + 80;
    min_z = last_raw_z - 80;
    max_z = last_raw_z + 80;
    bounds_inited = 1;
}

void mag_poll(void)
{
    uint8_t buf[6];
    int16_t rx, ry, rz;
    int32_t mx, my, mz;

    if (!state.sensor_ok) {
        probe_and_init_chip();
        if (!state.sensor_ok) {
            return;
        }
    }

    if (!i2c_bus_mem_read(mag_addr, out_reg, buf, 6u)) {
        state.sensor_ok = 0;
        return;
    }

    if (chip_type == CHIP_LSM303DLHC) {
        rx = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
        rz = (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
        ry = (int16_t)(((uint16_t)buf[4] << 8) | buf[5]);
        mx = ((int32_t)rx * 10) / 11;
        my = ((int32_t)ry * 10) / 11;
        mz = ((int32_t)rz * 10) / 10;
    } else {
        rx = (int16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
        ry = (int16_t)((uint16_t)buf[2] | ((uint16_t)buf[3] << 8));
        rz = (int16_t)((uint16_t)buf[4] | ((uint16_t)buf[5] << 8));

        int32_t sens_num = (chip_type == CHIP_LSM9DS0) ? 8 :
                           (chip_type == CHIP_LIS2MDL) ? 150 : 14;
        mx = ((int32_t)rx * sens_num) / 100;
        my = ((int32_t)ry * sens_num) / 100;
        mz = ((int32_t)rz * sens_num) / 100;
    }

    last_raw_x = mx;
    last_raw_y = my;
    last_raw_z = mz;

    if (!bounds_inited) {
        min_x = mx - 120;
        max_x = mx + 120;
        min_y = my - 120;
        max_y = my + 120;
        min_z = mz - 120;
        max_z = mz + 120;
        bounds_inited = 1;
    } else {
        if (mx < min_x) min_x = mx;
        if (mx > max_x) max_x = mx;
        if (my < min_y) min_y = my;
        if (my > max_y) max_y = my;
        if (mz < min_z) min_z = mz;
        if (mz > max_z) max_z = mz;

        if ((max_x - min_x) > 260 && (max_x - min_x) < 1200) {
            off_x = (min_x + max_x) / 2;
        }
        if ((max_y - min_y) > 260 && (max_y - min_y) < 1200) {
            off_y = (min_y + max_y) / 2;
        }
        if ((max_z - min_z) > 260 && (max_z - min_z) < 1200) {
            off_z = (min_z + max_z) / 2;
        }
    }

    mx -= off_x;
    my -= off_y;
    mz -= off_z;

    state.x_mgauss = (mx * (int32_t)state.scale_x) / 100;
    state.y_mgauss = (my * (int32_t)state.scale_y) / 100;
    state.z_mgauss = (mz * (int32_t)state.scale_z) / 100;

    float fx = (float)state.x_mgauss;
    float fy = (float)state.y_mgauss;
    float norm = sqrtf(fx * fx + fy * fy);

    if (norm > 1.0f) {
        /* На плате стенда ось X направлена вдоль корпуса, Y — поперёк;
         * при повороте стенда по часовой стрелке стрелка на экране поворачивается
         * в противоположную сторону, удерживая направление на север / магнит. */
        float angle_rad = atan2f(-fx, fy);
        float deg = angle_rad * (180.0f / 3.14159265f);
        if (deg < 0.0f) {
            deg += 360.0f;
        }
        state.heading_deg = (uint16_t)((int)deg % 360);
        state.vec_x = (int8_t)(sinf(angle_rad) * 18.0f);
        state.vec_y = (int8_t)(-cosf(angle_rad) * 18.0f);
    } else {
        state.heading_deg = 0;
        state.vec_x = 0;
        state.vec_y = -18;
    }
}

int mag_set_scales(uint16_t sx, uint16_t sy, uint16_t sz)
{
    if (sx > MAG_SCALE_MAX || sy > MAG_SCALE_MAX || sz > MAG_SCALE_MAX) {
        return 0;
    }
    state.scale_x = sx;
    state.scale_y = sy;
    state.scale_z = sz;
    return 1;
}

void mag_get_state(mag_state_t *out)
{
    if (out == 0) {
        return;
    }
    *out = state;
}
