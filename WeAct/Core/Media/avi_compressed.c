#include "avi_compressed.h"
#include <string.h>
uint32_t AVI_Read32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
void AVI_Write32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8)); }
uint32_t AVI_CompressedSize(const AVI_Info_t *i)
{
  uint64_t size = AVI_HEADER_SIZE + 8ULL + i->data_bytes + 16ULL * i->frames;
  return size <= AVI_MAX_SIZE ? (uint32_t)size : 0;
}
int AVI_CompressedHeader(uint8_t h[AVI_HEADER_SIZE], const AVI_Info_t *i)
{
  uint32_t size = AVI_CompressedSize(i);
  if (!size || !i->frames || !AVI_Header(h, &i->format, 1)) return 0;
  AVI_Write32(h + 4, size - 8);
  AVI_Write32(h + 36, i->max_frame * i->format.fps);
  AVI_Write32(h + 48, i->frames); AVI_Write32(h + 60, i->max_frame);
  memcpy(h + 112, "MJPG", 4);
  AVI_Write32(h + 140, i->frames); AVI_Write32(h + 144, i->max_frame);
  memcpy(h + 188, "MJPG", 4); AVI_Write32(h + 192, i->max_frame);
  AVI_Write32(h + 216, 4 + i->data_bytes);
  return 1;
}
int AVI_Info(const uint8_t h[AVI_HEADER_SIZE], AVI_Info_t *i)
{
  memset(i, 0, sizeof(*i));
  if (AVI_Parse(h, &i->format, &i->frames))
  {
    i->max_frame = AVI_FrameBytes(&i->format);
    i->data_bytes = i->frames * (i->max_frame + 8);
    return 1;
  }
  uint32_t w = AVI_Read32(h + 176), height = AVI_Read32(h + 180), fps = AVI_Read32(h + 132);
  if (!w || w > 320 || !height || height > 240 || !fps || fps > 60 || AVI_Read32(h + 216) < 4) return 0;
  i->format = (AVI_Format_t){(uint16_t)w, (uint16_t)height, (uint16_t)fps, AVI_MJPEG};
  i->frames = AVI_Read32(h + 48); i->max_frame = AVI_Read32(h + 144);
  i->data_bytes = AVI_Read32(h + 216) - 4;
  if (!i->max_frame || i->max_frame > 65536 || (i->data_bytes & 1) ||
      (uint64_t)i->frames * 12 > i->data_bytes) return 0;
  uint8_t expected[AVI_HEADER_SIZE];
  return AVI_CompressedHeader(expected, i) && !memcmp(h, expected, sizeof(expected));
}
void AVI_CompressedIndex(uint8_t h[16], uint32_t offset, uint32_t size)
{
  memcpy(h, "00dc", 4); AVI_Write32(h + 4, 0x10);
  AVI_Write32(h + 8, 4 + offset); AVI_Write32(h + 12, size);
}
