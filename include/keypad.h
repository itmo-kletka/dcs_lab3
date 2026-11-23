#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

/* Бонусное задание: матричная клавиатура 3x4 на расширителе PCA9538 (0xE2).
 * Коды клавиш 1..12, защита от дребезга, без переповторов,
 * при одновременном нажатии >1 кнопки считается, что ни одна не нажата. */

void keypad_init(void);
void keypad_poll(void);

/* Возвращает код 1..12 при новом нажатии, 0 если нажатий нет */
uint8_t keypad_take_key(void);

#endif
