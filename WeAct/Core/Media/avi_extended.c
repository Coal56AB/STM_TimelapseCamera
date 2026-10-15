#include "avi_extended.h"
#include <string.h>

static void Put32(uint8_t *p, uint32_t n) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8 * i)); }
static void Put64(uint8_t *p, uint64_t n) { Put32(p, (uint32_t)n); Put32(p + 4, (uint32_t)(n >> 32)); }
static uint32_t Capacity(const AVI_Format_t *f) { return 0x3f000000U / (AVI_FrameBytes(f) + 16U); }
static uint32_t Segments(const AVI_Format_t *f, uint32_t frames) { return (frames - 1) / Capacity(f) + 1; }
static uint32_t Header(const AVI_Format_t *f, uint32_t frames) { return 524 + 16 * Segments(f, frames); }
static uint32_t Count(const AVI_Format_t *f, uint32_t frames, uint32_t segment)
{
  uint32_t left = frames - segment * Capacity(f);
  return left < Capacity(f) ? left : Capacity(f);
}
static uint64_t Start(const AVI_Format_t *f, uint32_t frames, uint32_t segment)
{
  if (!segment) return 0;
  uint64_t full = 56ULL + (AVI_FrameBytes(f) + 16ULL) * Capacity(f);
  return Header(f, frames) - 24ULL + segment * full;
}
uint64_t AVI_ExtendedSize(const AVI_Format_t *f, uint32_t frames)
{
  if (!frames || !f->width || !f->height || !f->fps) return 0;
  uint32_t last = Segments(f, frames) - 1;
  return Start(f, frames, last) + (last ? 24 : Header(f, frames)) +
         32ULL + (AVI_FrameBytes(f) + 16ULL) * Count(f, frames, last);
}

int AVI_ExtendedRegion(const AVI_Format_t *f, uint32_t frames, uint64_t position,
                       uint8_t data[224], uint32_t *length, uint32_t *frame, uint32_t *offset)
{
  uint32_t chunk = AVI_FrameBytes(f) + 8, segments = Segments(f, frames), header = Header(f, frames);
  uint64_t first_end = header + 32ULL + (chunk + 8ULL) * Count(f, frames, 0);
  uint32_t segment = position < first_end ? 0 : 1 + (uint32_t)((position - first_end) / (56ULL + (chunk + 8ULL) * Capacity(f)));
  uint64_t start = Start(f, frames, segment);
  uint32_t local = (uint32_t)(position - start), count = Count(f, frames, segment), head = segment ? 24 : header;
  uint32_t index = head + count * chunk;
  uint8_t temp[224] = {0};
  uint32_t at, size;
  if (local >= head && local < index)
  {
    *frame = segment * Capacity(f) + (local - head) / chunk;
    *offset = (local - head) % chunk;
    *length = chunk - *offset;
    return 0;
  }
  if (local >= index)
  {
    if (local < index + 32)
    {
      memcpy(temp, "ix00", 4); Put32(temp + 4, 24 + count * 8);
      temp[8] = 2; temp[11] = 1; Put32(temp + 12, count); memcpy(temp + 16, "00db", 4);
      Put64(temp + 20, start + head);
      at = local - index; size = 32;
    }
    else
    {
      uint32_t entry = (local - index - 32) / 8;
      Put32(temp, 8 + entry * chunk); Put32(temp + 4, chunk - 8);
      at = (local - index - 32) % 8; size = 8;
    }
  }
  else if (segment)
  {
    memcpy(temp, "RIFF", 4); Put32(temp + 4, index + 32 + 8 * count - 8);
    memcpy(temp + 8, "AVIXLIST", 8); Put32(temp + 16, 4 + count * chunk); memcpy(temp + 20, "movi", 4);
    at = local; size = 24;
  }
  else if (local < 212)
  {
    AVI_Header(temp, f, count);
    Put32(temp + 4, index + 32 + 8 * count - 8);
    Put32(temp + 16, header - 32); Put32(temp + 92, 148 + 16 * segments);
    Put32(temp + 140, frames);
    at = local; size = 212;
  }
  else if (local < 244)
  {
    memcpy(temp, "indx", 4); Put32(temp + 4, 24 + 16 * segments);
    temp[8] = 4; Put32(temp + 12, segments); memcpy(temp + 16, "00db", 4);
    at = local - 212; size = 32;
  }
  else if (local < 244 + 16 * segments)
  {
    uint32_t entry = (local - 244) / 16, n = Count(f, frames, entry);
    Put64(temp, Start(f, frames, entry) + (entry ? 24 : header) + (uint64_t)n * chunk);
    Put32(temp + 8, 32 + n * 8); Put32(temp + 12, n);
    at = (local - 244) % 16; size = 16;
  }
  else if (local < header - 12)
  {
    uint32_t relative = local - (244 + 16 * segments);
    if (relative < 24)
    {
      memcpy(temp, "LIST", 4); Put32(temp + 4, 260); memcpy(temp + 8, "odmldmlh", 8);
      Put32(temp + 16, 248); Put32(temp + 20, frames);
      at = relative; size = 24;
    }
    else { at = 0; size = 268 - relative; if (size > sizeof(temp)) size = sizeof(temp); }
  }
  else
  {
    memcpy(temp, "LIST", 4); Put32(temp + 4, 4 + count * chunk); memcpy(temp + 8, "movi", 4);
    at = local - (header - 12); size = 12;
  }
  *length = size - at;
  memcpy(data, temp + at, *length);
  return 1;
}
