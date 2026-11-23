#ifndef OLED_H
#define OLED_H

#include <stdint.h>

/* OLED-дисплей WEO012864DL на контроллере SSD1306BZ (128x64, I2C адрес 0x78). */

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

void oled_init(void);
void oled_fill(uint8_t color);
void oled_update(void);

void oled_set_cursor(uint8_t x, uint8_t y);
void oled_draw_pixel(int x, int y, uint8_t color);
void oled_draw_line(int x0, int y0, int x1, int y1, uint8_t color);
void oled_draw_circle(int cx, int cy, int r, uint8_t color);
void oled_draw_arrow(int cx, int cy, int tip_x, int tip_y, uint8_t color);

void oled_write_char(char ch, uint8_t color);
void oled_write_str(const char *s, uint8_t color);
void oled_write_str_at(uint8_t x, uint8_t y, const char *s, uint8_t color);

#endif
