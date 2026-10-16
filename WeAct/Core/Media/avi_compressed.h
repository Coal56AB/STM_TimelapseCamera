#ifndef AVI_COMPRESSED_H
#define AVI_COMPRESSED_H
#include "avi.h"
typedef struct
{
  AVI_Format_t format;
  uint32_t frames, data_bytes, max_frame;
} AVI_Info_t;
uint32_t AVI_Read32(const uint8_t *p);
void AVI_Write32(uint8_t *p, uint32_t value);
uint32_t AVI_CompressedSize(const AVI_Info_t *info);
int AVI_CompressedHeader(uint8_t data[AVI_HEADER_SIZE], const AVI_Info_t *info);
int AVI_Info(const uint8_t data[AVI_HEADER_SIZE], AVI_Info_t *info);
void AVI_CompressedIndex(uint8_t data[16], uint32_t offset, uint32_t size);
#endif
