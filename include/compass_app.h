#ifndef COMPASS_APP_H
#define COMPASS_APP_H

/* Прикладная логика лабораторной работы 3, Вариант 2 «Компас»:
 * чтение магнитометра LSM9DS, отрисовка осей и стрелки на OLED,
 * обработка клавиатуры стенда и обмен с клиентской программой на ПК. */

void compass_app_init(void);
void compass_app_poll(void);
void compass_app_send_telemetry(void);

#endif
