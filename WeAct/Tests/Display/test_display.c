/* Проверка настоящего display_port: дисплей заменён RGB565-поверхностью. */
#include "display.h"
#include "st7735.h"
#include "spi.h"
#include "tim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

SPI_HandleTypeDef hspi4;
TIM_HandleTypeDef htim1;
static uint8_t source[320 * 240 * 2], screen[160 * 80 * 2], original[sizeof(screen)];
void HAL_GPIO_WritePin(int port, int pin, int value) { (void)port; (void)pin; (void)value; }
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, uint8_t *data, uint16_t size, uint32_t timeout)
{ (void)spi; (void)data; (void)size; (void)timeout; return HAL_OK; }
uint32_t HAL_GetTick(void) { return 0; }
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{ (void)timer; (void)channel; return HAL_OK; }
int32_t ST7735_RegisterBusIO(ST7735_Object_t *obj, ST7735_IO_t *io) { (void)obj; (void)io; return ST7735_OK; }
int32_t ST7735_Init(ST7735_Object_t *obj, uint32_t color, ST7735_Ctx_t *context)
{ (void)obj; (void)context; assert(color == ST7735_FORMAT_RBG565); return ST7735_OK; }
int32_t ST7735_FillRect(ST7735_Object_t *obj, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
  (void)obj; assert(x + w <= 160 && y + h <= 80);
  for (unsigned row = y; row < y + h; ++row) for (unsigned col = x; col < x + w; ++col)
  { unsigned at = (row * 160 + col) * 2; screen[at] = (uint8_t)(color >> 8); screen[at + 1] = (uint8_t)color; }
  return ST7735_OK;
}
int32_t ST7735_FillRGBRect(ST7735_Object_t *obj, uint32_t x, uint32_t y, uint8_t *data, uint32_t w, uint32_t h)
{
  (void)obj; assert(x + w <= 160 && y + h <= 80);
  for (unsigned row = 0; row < h; ++row) memcpy(screen + ((y + row) * 160 + x) * 2, data + row * w * 2, w * 2);
  return ST7735_OK;
}
static void Frame(uint16_t w, uint16_t h)
{
  for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x)
  { unsigned at = (y * w + x) * 2; source[at] = (uint8_t)y; source[at + 1] = (uint8_t)x; }
  Camera_Frame_t frame = {source, w, h};
  Display_Preview(&frame);
  unsigned scale = w / 160, top = (h - 80 * scale) / 2;
  for (unsigned y = 0; y < 80; ++y) for (unsigned x = 0; x < 160; ++x)
  { unsigned at = (y * 160 + x) * 2; assert(screen[at] == top + y * scale && screen[at + 1] == (uint8_t)(x * scale)); }
  memcpy(original, screen, sizeof(screen));
  /* Следующий DMA кадр не должен менять фон уже показанного предпросмотра. */
  memset(source, 0xa5, sizeof(source));
  Display_OverlayLine(0, "READY 5s #0", 0);
  Display_OverlayLine(5, "SD ERROR", 1);
  assert(memcmp(original, screen, 160 * 12 * 2) != 0);
  assert(memcmp(original + 65 * 320, screen + 65 * 320, 12 * 320) != 0);
  assert(memcmp(original + 12 * 320, screen + 12 * 320, 53 * 320) == 0);
  /* На следующем кадре HUD должен сохраниться без отдельного UI_Draw. */
  Display_Preview(&frame);
  unsigned changed = 0;
  for (unsigned i = 0; i < 12 * 320; ++i) changed += screen[i] != 0xa5;
  assert(changed > 0);
  Display_OverlayLine(0, "", 0); Display_OverlayLine(5, "", 0);
  for (unsigned i = 0; i < sizeof(screen); ++i) assert(screen[i] == 0xa5);
  /* Восстановление исходного кадра и очистка старой надписи. */
  for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x)
  { unsigned at = (y * w + x) * 2; source[at] = (uint8_t)y; source[at + 1] = (uint8_t)x; }
  Display_Preview(&frame);
  Display_OverlayLine(0, "", 0); Display_OverlayLine(5, "", 0);
  assert(memcmp(original, screen, sizeof(screen)) == 0);
}
int main(void)
{
  assert(Display_Init()); Frame(160, 120); Frame(320, 240);
  Display_Clear(); Display_OverlayLine(0, "", 0);
  for (unsigned i = 0; i < sizeof(screen); ++i) assert(screen[i] == 0);
  puts("PASS: full-screen center crop, RGB565 byte order, transparent HUD, background restore");
  return 0;
}
