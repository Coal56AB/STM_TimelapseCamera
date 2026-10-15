#include "avi.h"
#include <string.h>

static void Put16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
static void Put32(uint8_t *p, uint32_t n)
{
  Put16(p, (uint16_t)n);
  Put16(p + 2, (uint16_t)(n >> 16));
}
static uint32_t Get32(const uint8_t *p)
{
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint32_t AVI_FrameBytes(const AVI_Format_t *f)
{
  return (((uint32_t)f->width * 3U + 3U) & ~3U) * f->height;
}

uint32_t AVI_Size(const AVI_Format_t *f, uint32_t frames)
{
  uint64_t size = AVI_HEADER_SIZE + 8ULL + (AVI_FrameBytes(f) + 24ULL) * frames;
  return size <= AVI_MAX_SIZE ? (uint32_t)size : 0;
}

int AVI_Header(uint8_t h[AVI_HEADER_SIZE], const AVI_Format_t *f, uint32_t frames)
{
  uint32_t bytes = AVI_FrameBytes(f), size = AVI_Size(f, frames);
  if (!f->width || !f->height || !f->fps || !frames || !size) return 0;
  memset(h, 0, AVI_HEADER_SIZE);
  memcpy(h, "RIFF", 4); Put32(h + 4, size - 8); memcpy(h + 8, "AVI LIST", 8);
  Put32(h + 16, 192); memcpy(h + 20, "hdrlavih", 8); Put32(h + 28, 56);
  Put32(h + 32, 1000000U / f->fps); Put32(h + 36, bytes * f->fps);
  Put32(h + 44, 0x10); Put32(h + 48, frames); Put32(h + 56, 1);
  Put32(h + 60, bytes); Put32(h + 64, f->width); Put32(h + 68, f->height);
  memcpy(h + 88, "LIST", 4); Put32(h + 92, 116); memcpy(h + 96, "strlstrh", 8);
  Put32(h + 104, 56); memcpy(h + 108, "vidsDIB ", 8);
  Put32(h + 128, 1); Put32(h + 132, f->fps); Put32(h + 140, frames);
  Put32(h + 144, bytes); Put32(h + 148, 0xffffffffU);
  Put16(h + 160, f->width); Put16(h + 162, f->height);
  memcpy(h + 164, "strf", 4); Put32(h + 168, 40); Put32(h + 172, 40);
  Put32(h + 176, f->width); Put32(h + 180, f->height);
  Put16(h + 184, 1); Put16(h + 186, 24); Put32(h + 192, bytes);
  memcpy(h + 212, "LIST", 4); Put32(h + 216, 4 + frames * (bytes + 8));
  memcpy(h + 220, "movi", 4);
  return 1;
}

int AVI_Parse(const uint8_t h[AVI_HEADER_SIZE], AVI_Format_t *f, uint32_t *frames)
{
  uint8_t expected[AVI_HEADER_SIZE];
  uint32_t w = Get32(h + 176), height = Get32(h + 180), fps = Get32(h + 132);
  if (w > 65535 || height > 65535 || fps > 60) return 0;
  f->width = (uint16_t)w; f->height = (uint16_t)height; f->fps = (uint16_t)fps;
  *frames = Get32(h + 48);
  return AVI_Header(expected, f, *frames) && !memcmp(h, expected, AVI_HEADER_SIZE);
}

void AVI_ChunkHeader(uint8_t h[8], const AVI_Format_t *f)
{
  memcpy(h, "00db", 4); Put32(h + 4, AVI_FrameBytes(f));
}
void AVI_IndexHeader(uint8_t h[8], uint32_t frames)
{
  memcpy(h, "idx1", 4); Put32(h + 4, frames * 16U);
}
void AVI_IndexEntry(uint8_t h[16], const AVI_Format_t *f, uint32_t frame)
{
  memcpy(h, "00db", 4); Put32(h + 4, 0x10);
  Put32(h + 8, 4 + frame * (AVI_FrameBytes(f) + 8)); Put32(h + 12, AVI_FrameBytes(f));
}
void AVI_ConvertRow(uint8_t *bgr, const uint8_t *rgb, uint16_t width)
{
  for (uint16_t x = 0; x < width; ++x)
  {
    uint16_t p = (uint16_t)((uint16_t)rgb[x * 2] << 8 | rgb[x * 2 + 1]);
    uint8_t r = (uint8_t)(p >> 11), g = (uint8_t)((p >> 5) & 63), b = (uint8_t)(p & 31);
    bgr[x * 3] = (uint8_t)((b << 3) | (b >> 2));
    bgr[x * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
    bgr[x * 3 + 2] = (uint8_t)((r << 3) | (r >> 2));
  }
  for (uint32_t i = width * 3U; i < ((width * 3U + 3U) & ~3U); ++i) bgr[i] = 0;
}
