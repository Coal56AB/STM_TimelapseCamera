#ifndef H264_FRAME_H
#define H264_FRAME_H
#include <stdint.h>
int H264_Begin(uint16_t width, uint16_t height, uint16_t fps);
/* Annex B output remains valid until the next capture/encode. */
int H264_Frame(const uint8_t *rgb565_be, const char *timestamp, uint8_t **data, uint32_t *size);
#endif
