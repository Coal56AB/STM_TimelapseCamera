#include "st7735.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t command, address[4], column[4], row[4], pixels[25600];
static unsigned address_at, transfers;
static uint32_t bytes;
static int checking;
static int32_t Init(void) { return 0; }
static int32_t Tick(void) { static int32_t tick; return ++tick; }
static int32_t Write(uint8_t reg, uint8_t *data, uint32_t length)
{
  (void)data; (void)length; command = reg; address_at = 0; return 0;
}
static int32_t Send(uint8_t *data, uint32_t length)
{
  if (command == ST7735_CASET || command == ST7735_RASET)
  {
    assert(address_at + length <= 4);
    memcpy(address + address_at, data, length); address_at += length;
    if (address_at == 4) memcpy(command == ST7735_CASET ? column : row, address, 4);
  }
  else if (checking && command == ST7735_WRITE_RAM)
  {
    const uint8_t expected_column[] = {0, 1, 0, 160}, expected_row[] = {0, 26, 0, 105};
    assert(!memcmp(column, expected_column, 4) && !memcmp(row, expected_row, 4));
    assert(length == sizeof(pixels) && !memcmp(data, pixels, length));
    ++transfers; bytes += length;
  }
  return 0;
}
int main(void)
{
  ST7735_Object_t panel = {0}; ST7735_IO_t io = {Init, NULL, 0, Write, NULL, Send, NULL, Tick};
  ST7735_Ctx_t context = {0}; context.Orientation = ST7735_ORIENTATION_LANDSCAPE_ROT180;
  context.Panel = HannStar_Panel; context.Type = ST7735_0_9_inch_screen;
  assert(ST7735_RegisterBusIO(&panel, &io) == ST7735_OK);
  assert(ST7735_Init(&panel, ST7735_FORMAT_RBG565, &context) == ST7735_OK);
  for (unsigned i = 0; i < sizeof(pixels); ++i) pixels[i] = (uint8_t)(i * 13);
  checking = 1;
  assert(ST7735_FillRGBRect(&panel, 0, 0, pixels, 160, 80) == ST7735_OK);
  assert(transfers == 1 && bytes == 25600);
  assert(ST7735_FillRGBRect(&panel, 1, 0, pixels, 160, 80) == ST7735_ERROR);
  assert(transfers == 1);
  puts("PASS: real ST7735 driver, full-screen window, contiguous RGB565 transfer, bounds");
  return 0;
}
