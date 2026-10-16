/** @brief Штатный ST7735 160x80, SPI4, подсветка TIM1_CH2N. */
#include "display.h"
#include "st7735.h"
#include "font.h"
#include "spi.h"
#include "tim.h"
#include <string.h>

static ST7735_Object_t panel;
static uint8_t stripe[160 * 12 * 2];
/* Копия предпросмотра позволяет обновлять надписи во время следующего DMA кадра. */
static uint8_t preview[160 * 80 * 2];
static struct { char text[27]; uint8_t active, highlighted; } overlays[6];
static struct { char text[27]; uint8_t valid, selected; } lines[6];

static int32_t Init(void)
{
  if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2) != HAL_OK) return -1;
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 100);
  return 0;
}
static int32_t Tick(void) { return (int32_t)HAL_GetTick(); }
static int32_t Send(uint8_t *data, uint32_t length)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin, GPIO_PIN_SET);
  HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi4, data, (uint16_t)length, 100);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  return result == HAL_OK ? 0 : -1;
}
static int32_t Write(uint8_t reg, uint8_t *data, uint32_t length)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi4, &reg, 1, 100);
  HAL_GPIO_WritePin(LCD_WR_RS_GPIO_Port, LCD_WR_RS_Pin, GPIO_PIN_SET);
  if (result == HAL_OK && length) result = HAL_SPI_Transmit(&hspi4, data, (uint16_t)length, 100);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  return result == HAL_OK ? 0 : -1;
}
int Display_Init(void)
{
  ST7735_IO_t io = {Init, NULL, 0, Write, NULL, Send, NULL, Tick};
  ST7735_Ctx_t context = {0};
  context.Orientation = ST7735_ORIENTATION_LANDSCAPE_ROT180;
  context.Panel = HannStar_Panel;
  context.Type = ST7735_0_9_inch_screen;
  if (ST7735_RegisterBusIO(&panel, &io) != ST7735_OK ||
      ST7735_Init(&panel, ST7735_FORMAT_RBG565, &context) != ST7735_OK) return 0;
  Display_Clear();
  return 1;
}
void Display_Clear(void)
{
  memset(preview, 0, sizeof(preview));
  memset(overlays, 0, sizeof(overlays));
  memset(lines, 0, sizeof(lines));
  ST7735_FillRect(&panel, 0, 0, 160, 80, 0);
}
static void DrawLine(uint8_t row, const char *text, uint8_t selected, uint8_t overlay)
{
  if (row > 5) return;
  uint16_t foreground = overlay ? (selected ? 0xffe0 : 0xffff) : (selected ? 0 : 0xffff);
  uint16_t background = selected ? 0x07ff : 0;
  if (overlay) memcpy(stripe, preview + row * 13 * 160 * 2, sizeof(stripe));
  else for (unsigned i = 0; i < 160 * 12; ++i)
  {
    stripe[i * 2] = (uint8_t)(background >> 8); stripe[i * 2 + 1] = (uint8_t)background;
  }
  /* Чёрная тень сохраняет читаемость светлого текста на ярком кадре. */
  for (unsigned pass = overlay ? 0 : 1; pass < 2; ++pass)
  for (unsigned col = 0; col < 26 && text[col]; ++col)
  {
    unsigned ch = (unsigned char)text[col];
    if (ch < 32 || ch > 126) ch = '?';
    for (unsigned x = 0; x < 6; ++x)
      for (unsigned y = 0; y < 12; ++y)
        if (asc2_1206[ch - 32][x * 2 + y / 8] & (0x80U >> (y % 8)))
        {
          unsigned px = col * 6 + x + (pass == 0), py = y + (pass == 0);
          if (px >= 160 || py >= 12) continue;
          unsigned at = (py * 160 + px) * 2;
          uint16_t color = pass ? foreground : 0;
          stripe[at] = (uint8_t)(color >> 8); stripe[at + 1] = (uint8_t)color;
        }
  }
  ST7735_FillRGBRect(&panel, 0, row * 13, stripe, 160, 12);
}
void Display_Line(uint8_t row, const char *text, uint8_t selected)
{
  if (row > 5) return;
  if (lines[row].valid && lines[row].selected == selected && !strncmp(lines[row].text, text, 26)) return;
  strncpy(lines[row].text, text, 26); lines[row].text[26] = 0;
  lines[row].valid = 1; lines[row].selected = selected;
  DrawLine(row, text, selected, 0);
}
void Display_OverlayLine(uint8_t row, const char *text, uint8_t highlighted)
{
  if (row > 5) return;
  lines[row].valid = 0;
  strncpy(overlays[row].text, text, sizeof(overlays[row].text) - 1);
  overlays[row].text[26] = '\0';
  overlays[row].active = 1; overlays[row].highlighted = highlighted;
  DrawLine(row, overlays[row].text, highlighted, 1);
}
void Display_Preview(const Camera_Frame_t *frame)
{
  if (!frame || !frame->rgb565_be || !frame->width || !frame->height) return;
  memset(lines, 0, sizeof(lines));
  /* Заполнение экрана с центральной обрезкой, без растяжения пропорций. */
  unsigned crop_width = frame->width, crop_height = frame->height;
  if (crop_width * 80 > crop_height * 160) crop_width = crop_height * 160 / 80;
  else crop_height = crop_width * 80 / 160;
  unsigned left = (frame->width - crop_width) / 2, top = (frame->height - crop_height) / 2;
  for (unsigned y = 0; y < 80; ++y)
  {
    for (unsigned x = 0; x < 160; ++x)
    {
      unsigned source = ((top + y * crop_height / 80) * frame->width + left + x * crop_width / 160) * 2;
      unsigned at = (y * 160 + x) * 2;
      preview[at] = frame->rgb565_be[source]; preview[at + 1] = frame->rgb565_be[source + 1];
    }
  }
  /* Передаём новый кадр уже с надписями: они не исчезают на время записи SD. */
  for (unsigned row = 0; row < 6; ++row)
  {
    unsigned y = row * 13;
    if (overlays[row].active)
    {
      DrawLine((uint8_t)row, overlays[row].text, overlays[row].highlighted, 1);
      ST7735_FillRGBRect(&panel, 0, y + 12, preview + (y + 12) * 320, 160, 1);
    }
    else ST7735_FillRGBRect(&panel, 0, y, preview + y * 320, 160, 13);
  }
  ST7735_FillRGBRect(&panel, 0, 78, preview + 78 * 320, 160, 2);
}
