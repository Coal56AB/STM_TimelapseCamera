#ifndef AVI_EXTENDED_H
#define AVI_EXTENDED_H
#include "avi.h"
uint64_t AVI_ExtendedSize(const AVI_Format_t *format, uint32_t frames);
/* 1: bytes generated in data; 0: read source frame at offset. */
int AVI_ExtendedRegion(const AVI_Format_t *format, uint32_t frames, uint64_t position,
                       uint8_t data[224], uint32_t *length, uint32_t *frame, uint32_t *offset);
#endif
