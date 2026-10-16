#ifndef JPEG_FRAME_H
#define JPEG_FRAME_H
#include <stdint.h>
/* RGB565 big-endian input. Returned buffer is valid until the next encode. */
int JPEG_Frame(const uint8_t *rgb, uint16_t width, uint16_t height,
               const char *timestamp, const uint8_t **data, uint32_t *size);
#endif
