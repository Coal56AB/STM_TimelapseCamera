#include "button.h"
#include <string.h>

void Button_Init(Button_t *button, const Button_Config_t *config)
{
  memset(button, 0, sizeof(*button));
  button->config = *config;
}

Button_Event_t Button_Process(Button_t *b, uint8_t pressed, uint32_t now)
{
  Button_Event_t event = BUTTON_NONE;
  pressed = !!pressed;
  if (pressed != b->raw)
  {
    b->raw = pressed;
    b->changed_at = now;
  }
  if (b->stable != b->raw && now - b->changed_at >= b->config.debounce_ms)
  {
    b->stable = b->raw;
    if (b->stable)
    {
      b->pressed_at = now;
      b->held = 0;
    }
    else if (!b->held)
    {
      if (b->pending && b->pressed_at - b->released_at <= b->config.double_ms)
      {
        b->pending = 0;
        event = BUTTON_DOUBLE;
      }
      else
      {
        b->pending = 1;
        b->released_at = now;
      }
    }
  }
  if (b->stable && !b->held && now - b->pressed_at >= b->config.long_ms)
  {
    b->held = 1;
    b->pending = 0;
    return BUTTON_LONG;
  }
  if (b->pending && !b->stable && !b->raw && now - b->released_at > b->config.double_ms)
  {
    b->pending = 0;
    return BUTTON_CLICK;
  }
  return event;
}
