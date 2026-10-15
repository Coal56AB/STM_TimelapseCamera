#ifndef IMAGE_TEXT_H
#define IMAGE_TEXT_H
#include <stdint.h>
/* Накладывает текст на одну BGR24-строку; y — координата сверху полного кадра. */
void ImageText_Row(uint8_t *bgr, uint16_t width, uint16_t height, uint16_t y, const char *text);
#endif
