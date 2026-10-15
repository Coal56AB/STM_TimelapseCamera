#include "image_text.h"
#include "font.h"
void ImageText_Row(uint8_t *bgr, uint16_t width, uint16_t height, uint16_t y, const char *text)
{
  if (!text || height < 16 || y < height - 15 || y >= height - 2) return;
  /* Нижний левый угол: белый шрифт 6x12, чёрная тень, отступ 2 пикселя. */
  for (unsigned pass = 0; pass < 2; ++pass)
  {
    unsigned shift = pass == 0, py = y - (height - 15);
    if (py < shift || py - shift >= 12) continue;
    py -= shift;
    for (unsigned col = 0; text[col] && 2 + col * 6 < width; ++col)
    {
      unsigned ch = (unsigned char)text[col];
      if (ch < 32 || ch > 126) ch = '?';
      for (unsigned x = 0; x < 6; ++x)
      {
        unsigned px = 2 + col * 6 + x + shift;
        if (px < width && (asc2_1206[ch - 32][x * 2 + py / 8] & (0x80U >> (py % 8))))
          for (unsigned c = 0; c < 3; ++c) bgr[px * 3 + c] = pass ? 255 : 0;
      }
    }
  }
}
