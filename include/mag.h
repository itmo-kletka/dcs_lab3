#ifndef MAG_H
#define MAG_H

#include <stdint.h>

/* Магнитометр инерциального модуля LSM9DS (LSM9DS1 / LSM9DS0) на шине I2C.
 * Масштабные коэффициенты для каждой оси: 0..200 %, по умолчанию 100 %. */

#define MAG_SCALE_MIN  0u
#define MAG_SCALE_MAX  200u
#define MAG_SCALE_INIT 100u

typedef struct {
    int32_t x_mgauss;
    int32_t y_mgauss;
    int32_t z_mgauss;
    uint16_t scale_x;
    uint16_t scale_y;
    uint16_t scale_z;
    uint16_t heading_deg; /* 0..359 градусов */
    int8_t vec_x;         /* проекция стрелки -18..+18 пикселей */
    int8_t vec_y;         /* проекция стрелки -18..+18 пикселей */
    uint8_t sensor_ok;
} mag_state_t;

void mag_init(void);
void mag_poll(void);

int mag_set_scales(uint16_t sx, uint16_t sy, uint16_t sz);
void mag_calibrate_zero(void);
void mag_get_state(mag_state_t *out);

#endif
