#include "mp4.h"
#include <string.h>
static void Put(uint8_t *p, uint32_t n) { p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n; }
static uint32_t Get(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static unsigned Box(uint8_t *b, unsigned *at, const char *tag, unsigned bytes)
{ unsigned start = *at; Put(b + start, bytes); memcpy(b + start + 4, tag, 4); *at += bytes; return start; }
static void Matrix(uint8_t *p) { Put(p, 0x10000); Put(p + 16, 0x10000); Put(p + 32, 0x40000000); }

int MP4_Header(uint8_t b[MP4_HEADER_SIZE], const MP4_Info_t *i)
{
  if (!i->format.width || i->format.width > 320 || !i->format.height || i->format.height > 240 ||
      !i->format.fps || i->format.fps > 60 || i->sps_size < 4 || i->sps_size > 64 || !i->pps_size || i->pps_size > 64) return 0;
  memset(b, 0, MP4_HEADER_SIZE);
  unsigned at = 0, p;
  Box(b, &at, "ftyp", 32); memcpy(b + 8, "isom", 4); Put(b + 12, 512); memcpy(b + 16, "isomiso6avc1mp41", 16);
  unsigned moov = Box(b, &at, "moov", 8);
  p = Box(b, &at, "mvhd", 108); Put(b + p + 20, i->format.fps); Put(b + p + 24, i->frames);
  Put(b + p + 28, 0x10000); b[p + 32] = 1; Matrix(b + p + 44); Put(b + p + 104, 2);
  unsigned trak = Box(b, &at, "trak", 8);
  p = Box(b, &at, "tkhd", 92); Put(b + p + 8, 7); Put(b + p + 20, 1); Put(b + p + 28, i->frames);
  Matrix(b + p + 48); Put(b + p + 84, (uint32_t)i->format.width << 16); Put(b + p + 88, (uint32_t)i->format.height << 16);
  unsigned mdia = Box(b, &at, "mdia", 8);
  p = Box(b, &at, "mdhd", 32); Put(b + p + 20, i->format.fps); Put(b + p + 24, i->frames); b[p + 28] = 0x55; b[p + 29] = 0xc4;
  p = Box(b, &at, "hdlr", 40); memcpy(b + p + 16, "vide", 4); memcpy(b + p + 32, "Camera", 6);
  unsigned minf = Box(b, &at, "minf", 8);
  p = Box(b, &at, "vmhd", 20); Put(b + p + 8, 1);
  p = Box(b, &at, "dinf", 36); Put(b + p + 8, 28); memcpy(b + p + 12, "dref", 4); Put(b + p + 20, 1);
  Put(b + p + 24, 12); memcpy(b + p + 28, "url ", 4); Put(b + p + 32, 1);
  unsigned stbl = Box(b, &at, "stbl", 8), stsd = Box(b, &at, "stsd", 16);
  Put(b + stsd + 12, 1);
  unsigned avc1 = Box(b, &at, "avc1", 86); b[avc1 + 15] = 1;
  b[avc1 + 32] = i->format.width >> 8; b[avc1 + 33] = i->format.width;
  b[avc1 + 34] = i->format.height >> 8; b[avc1 + 35] = i->format.height;
  Put(b + avc1 + 36, 0x480000); Put(b + avc1 + 40, 0x480000); b[avc1 + 49] = 1;
  b[avc1 + 83] = 24; b[avc1 + 84] = b[avc1 + 85] = 255;
  p = Box(b, &at, "avcC", 19 + i->sps_size + i->pps_size);
  b[p + 8] = 1; memcpy(b + p + 9, i->sps + 1, 3); b[p + 12] = 255; b[p + 13] = 225;
  b[p + 15] = i->sps_size; memcpy(b + p + 16, i->sps, i->sps_size);
  b[p + 16 + i->sps_size] = 1; b[p + 18 + i->sps_size] = i->pps_size;
  memcpy(b + p + 19 + i->sps_size, i->pps, i->pps_size);
  Put(b + avc1, at - avc1); Put(b + stsd, at - stsd);
  Box(b, &at, "stts", 16); Box(b, &at, "stsc", 16); Box(b, &at, "stsz", 20); Box(b, &at, "stco", 16);
  Put(b + stbl, at - stbl); Put(b + minf, at - minf); Put(b + mdia, at - mdia); Put(b + trak, at - trak);
  p = Box(b, &at, "mvex", 40); Put(b + p + 8, 32); memcpy(b + p + 12, "trex", 4);
  Put(b + p + 20, 1); Put(b + p + 24, 1); Put(b + p + 28, 1);
  Put(b + moov, at - moov); Box(b, &at, "free", MP4_HEADER_SIZE - at);
  return 1;
}

static unsigned Find(const uint8_t *b, unsigned begin, unsigned end, const char *tag)
{
  for (unsigned at = begin; at + 8 <= end;)
  { uint32_t size = Get(b + at); if (size < 8 || size > end - at) return 0; if (!memcmp(b + at + 4, tag, 4)) return at; at += size; }
  return 0;
}
int MP4_Parse(const uint8_t b[MP4_HEADER_SIZE], MP4_Info_t *i)
{
  unsigned mvhd = Find(b, 40, MP4_HEADER_SIZE, "mvhd"), trak = Find(b, 40, MP4_HEADER_SIZE, "trak");
  if (!mvhd || !trak) return 0;
  unsigned mdia = Find(b, trak + 8, trak + Get(b + trak), "mdia"); if (!mdia) return 0;
  unsigned minf = Find(b, mdia + 8, mdia + Get(b + mdia), "minf"); if (!minf) return 0;
  unsigned stbl = Find(b, minf + 8, minf + Get(b + minf), "stbl"); if (!stbl) return 0;
  unsigned stsd = Find(b, stbl + 8, stbl + Get(b + stbl), "stsd"); if (!stsd || stsd + 102 > MP4_HEADER_SIZE) return 0;
  unsigned avc = stsd + 16, conf = avc + 86;
  if (conf + 19 >= MP4_HEADER_SIZE || memcmp(b + avc + 4, "avc1", 4) || memcmp(b + conf + 4, "avcC", 4)) return 0;
  memset(i, 0, sizeof(*i));
  i->format = (AVI_Format_t){(uint16_t)(b[avc + 32] * 256 + b[avc + 33]), (uint16_t)(b[avc + 34] * 256 + b[avc + 35]), (uint16_t)Get(b + mvhd + 20), AVI_H264};
  i->frames = Get(b + mvhd + 24);
  i->sps_size = b[conf + 15]; if (i->sps_size > 64 || conf + 19U + i->sps_size >= MP4_HEADER_SIZE) return 0;
  i->pps_size = b[conf + 18 + i->sps_size];
  if (i->pps_size > 64 || conf + 19U + i->sps_size + i->pps_size > MP4_HEADER_SIZE) return 0;
  memcpy(i->sps, b + conf + 16, i->sps_size); memcpy(i->pps, b + conf + 19 + i->sps_size, i->pps_size);
  uint8_t expected[MP4_HEADER_SIZE];
  return i->frames && MP4_Header(expected, i) && !memcmp(b, expected, sizeof(expected));
}

void MP4_Packet(uint8_t b[MP4_PACKET_SIZE], uint32_t frame, uint32_t size, uint8_t key)
{
  memset(b, 0, MP4_PACKET_SIZE); unsigned at = 0, p;
  Box(b, &at, "moof", 8); Put(b, 100);
  p = Box(b, &at, "mfhd", 16); Put(b + p + 12, frame + 1);
  p = Box(b, &at, "traf", 8); Put(b + p, 76);
  p = Box(b, &at, "tfhd", 16); Put(b + p + 8, 0x20000); Put(b + p + 12, 1);
  p = Box(b, &at, "tfdt", 20); b[p + 8] = 1; Put(b + p + 16, frame);
  p = Box(b, &at, "trun", 32); Put(b + p + 8, 0x701); Put(b + p + 12, 1); Put(b + p + 16, MP4_PACKET_SIZE);
  Put(b + p + 20, 1); Put(b + p + 24, size); Put(b + p + 28, key ? 0x2000000 : 0x1010000);
  p = Box(b, &at, "mdat", 8); Put(b + p, size + 8);
}
int MP4_ParsePacket(const uint8_t b[MP4_PACKET_SIZE], uint32_t frame, uint32_t *size, uint8_t *key)
{
  *size = Get(b + 92); *key = Get(b + 96) == 0x2000000;
  if (!*size || *size > 182656) return 0;
  uint8_t expected[MP4_PACKET_SIZE]; MP4_Packet(expected, frame, *size, *key);
  return !memcmp(b, expected, sizeof(expected));
}
int MP4_Sample(MP4_Info_t *i, uint8_t *b, uint32_t *size, uint8_t *key)
{
  uint32_t at = 0, out = 0, original = *size; *key = 0;
  while (at + 4 < *size)
  {
    if (Get(b + at) != 1) return 0;
    uint32_t end = at + 5; while (end + 4 <= *size && Get(b + end) != 1) ++end;
    if (end + 4 > *size) end = *size;
    uint32_t n = end - at - 4; uint8_t kind = b[at + 4] & 31;
    if (kind == 7 || kind == 8)
    {
      if (n > 64) return 0;
      memcpy(kind == 7 ? i->sps : i->pps, b + at + 4, n);
      if (kind == 7) i->sps_size = n; else i->pps_size = n;
    }
    else
    { memmove(b + out + 4, b + at + 4, n); Put(b + out, n); out += n + 4; if (kind == 5) *key = 1; }
    at = end;
  }
  *size = out; return out && at == original;
}
