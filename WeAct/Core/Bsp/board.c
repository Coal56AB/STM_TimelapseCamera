#include "board.h"
#include "main.h"
static Button_t button;
static volatile Button_Event_t events[8];
static volatile uint8_t head, tail, ready;

void Board_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  const Button_Config_t config = {25, 300, 800};
  __HAL_RCC_GPIOC_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_13; gpio.Mode = GPIO_MODE_INPUT; gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &gpio);
  gpio.Pin = GPIO_PIN_3; gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &gpio);
  Board_Led(0);
  Button_Init(&button, &config);
  ready = 1;
}
void Board_Tick(void)
{
  if (!ready) return;
  Button_Event_t event = Button_Process(&button, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET, HAL_GetTick());
  uint8_t next = (uint8_t)((head + 1) & 7U);
  if (event != BUTTON_NONE && next != tail) { events[head] = event; head = next; }
}
Button_Event_t Board_Button(void)
{
  if (tail == head) return BUTTON_NONE;
  Button_Event_t event = events[tail];
  tail = (uint8_t)((tail + 1) & 7U);
  return event;
}
uint32_t Board_Time(void) { return HAL_GetTick(); }
void Board_Led(uint8_t on) { HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
