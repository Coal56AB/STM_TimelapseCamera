/** @file avi.h
 *  @brief AVI/DIB: заголовок, индекс и преобразование RGB565 в BGR24.
 *  @details Контейнер не зависит от камеры и файловой системы.
 */
#ifndef AVI_H
#define AVI_H
#include <stdint.h>
#define AVI_HEADER_SIZE 224U
#define AVI_MAX_SIZE 0x7fffffffUL
enum { AVI_DIB = 0, AVI_MJPEG = 1 };
typedef struct { uint16_t width, height, fps, codec; } AVI_Format_t;
uint32_t AVI_FrameBytes(const AVI_Format_t *format);
uint32_t AVI_Size(const AVI_Format_t *format, uint32_t frames);
int AVI_Header(uint8_t data[AVI_HEADER_SIZE], const AVI_Format_t *format, uint32_t frames);
int AVI_Parse(const uint8_t data[AVI_HEADER_SIZE], AVI_Format_t *format, uint32_t *frames);
void AVI_IndexEntry(uint8_t data[16], const AVI_Format_t *format, uint32_t frame);
void AVI_ChunkHeader(uint8_t data[8], const AVI_Format_t *format);
void AVI_IndexHeader(uint8_t data[8], uint32_t frames);
void AVI_ConvertRow(uint8_t *bgr, const uint8_t *rgb565_be, uint16_t width);
#endif
