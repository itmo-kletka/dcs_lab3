#include "cli.h"
#include "uart.h"
#include "mag.h"
#include "i2c_bus.h"
#include "compass_app.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Протокол обмена с клиентской программой на ПК по UART:
 *   PING                 -> PONG
 *   GET                  -> DATA X=... Y=... Z=... SX=... SY=... SZ=... DEG=...
 *   SCALE <sx> <sy> <sz> -> OK (или ERR) + свежий кадр DATA */

#define LINE_MAX 64

static char line[LINE_MAX];
static uint8_t line_len;

static char *trim(char *s)
{
    char *end;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
        *end = 0;
    }
    return s;
}

static int parse_three_u16(const char *s, uint16_t *a, uint16_t *b, uint16_t *c)
{
    char *end;
    unsigned long v1, v2, v3;

    v1 = strtoul(s, &end, 10);
    if (end == s || v1 > MAG_SCALE_MAX) {
        return 0;
    }
    s = end;
    v2 = strtoul(s, &end, 10);
    if (end == s || v2 > MAG_SCALE_MAX) {
        return 0;
    }
    s = end;
    v3 = strtoul(s, &end, 10);
    if (end == s || v3 > MAG_SCALE_MAX) {
        return 0;
    }
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    if (*end != 0) {
        return 0;
    }

    *a = (uint16_t)v1;
    *b = (uint16_t)v2;
    *c = (uint16_t)v3;
    return 1;
}

static void on_line(void)
{
    char *s;

    line[line_len] = 0;
    s = trim(line);
    if (*s == 0) {
        return;
    }

    if (strcmp(s, "PING") == 0) {
        uint16_t a;
        char msg[48];

        uart_write_str("PONG\r\n");
        uart_write_str("I2C_SCAN:");
        for (a = 0x10u; a <= 0xFEu; a += 2u) {
            if (i2c_bus_ready(a)) {
                uint8_t r0f = 0;
                (void)i2c_bus_read_reg(a, 0x0Fu, &r0f);
                snprintf(msg, sizeof(msg), " 0x%02X(0F=%02X)", (unsigned)a, (unsigned)r0f);
                uart_write_str(msg);
            }
        }
        uart_write_str("\r\n");
        compass_app_send_telemetry();
        return;
    }

    if (strcmp(s, "GET") == 0) {
        compass_app_send_telemetry();
        return;
    }

    if (strcmp(s, "CAL") == 0) {
        mag_calibrate_zero();
        uart_write_str("OK\r\n");
        compass_app_send_telemetry();
        return;
    }

    if (strncmp(s, "SCALE", 5) == 0 && (s[5] == ' ' || s[5] == '\t')) {
        uint16_t sx, sy, sz;

        if (parse_three_u16(s + 5, &sx, &sy, &sz) && mag_set_scales(sx, sy, sz)) {
            uart_write_str("OK\r\n");
            compass_app_send_telemetry();
        } else {
            uart_write_str("ERR\r\n");
        }
        return;
    }

    uart_write_str("ERR\r\n");
}

void cli_init(void)
{
    line_len = 0;
    uart_write_str("READY COMPASS\r\n");
}

void cli_poll(void)
{
    uint8_t byte;

    while (uart_read(&byte)) {
        if (byte == '\r' || byte == '\n') {
            if (line_len > 0u) {
                on_line();
                line_len = 0;
            }
            continue;
        }
        if (byte >= 0x20u && byte <= 0x7eu && line_len + 1u < LINE_MAX) {
            line[line_len++] = (char)byte;
        }
    }
}
