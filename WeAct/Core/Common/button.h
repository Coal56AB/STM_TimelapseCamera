/** @file button.h
 *  @brief Распознавание одиночного, двойного и долгого нажатия.
 */
#ifndef BUTTON_H
#define BUTTON_H
#include <stdint.h>

typedef enum { BUTTON_NONE, BUTTON_CLICK, BUTTON_DOUBLE, BUTTON_LONG } Button_Event_t;
typedef struct
{
  uint32_t debounce_ms, double_ms, long_ms;
} Button_Config_t;
typedef struct
{
  Button_Config_t config;
  uint32_t changed_at, pressed_at, released_at;
  uint8_t raw, stable, pending, held;
} Button_t;

void Button_Init(Button_t *button, const Button_Config_t *config);
Button_Event_t Button_Process(Button_t *button, uint8_t pressed, uint32_t now);
#endif
