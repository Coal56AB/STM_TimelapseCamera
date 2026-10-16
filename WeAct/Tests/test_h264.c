#include "h264_frame.h"
#include "media_memory.h"
#include "mp4.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
  for (unsigned w = 160; w <= 320; w *= 2)
  {
    unsigned h = w * 3 / 4;
    assert(H264_Begin(w, h, 25));
    char path[64]; snprintf(path, sizeof(path), "h264-%u.264", w);
    FILE *file = fopen(path, "wb"); assert(file);
    snprintf(path, sizeof(path), "h264-%u.mp4", w);
    FILE *mp4 = fopen(path, "wb+"); assert(mp4);
    uint8_t header[MP4_HEADER_SIZE] = {0}; assert(fwrite(header, 1, sizeof(header), mp4) == sizeof(header));
    MP4_Info_t info = {0}; info.format = (AVI_Format_t){w, h, 25, AVI_H264};
    for (unsigned frame = 0; frame < 30; ++frame)
    {
      uint8_t *rgb = MEDIA_CAPTURE_BUFFER;
      for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x)
      {
        uint16_t color = y < h / 2 ? 0xf800 : 0x07ff;
        if (x >= frame && x < frame + 8) color = 0xffff;
        unsigned at = (y * w + x) * 2; rgb[at] = color >> 8; rgb[at + 1] = color;
      }
      uint8_t *data; uint32_t size;
      assert(H264_Frame(rgb, "16.10.2026 12:34:56", &data, &size));
      assert(fwrite(data, 1, size, file) == size);
      uint8_t key;
      assert(MP4_Sample(&info, data, &size, &key));
      assert(key == (frame % 25 == 0));
      MP4_Packet(header, frame, size, key);
      uint32_t checked; uint8_t checked_key;
      assert(MP4_ParsePacket(header, frame, &checked, &checked_key) && checked == size && checked_key == key);
      assert(fwrite(header, 1, MP4_PACKET_SIZE, mp4) == MP4_PACKET_SIZE && fwrite(data, 1, size, mp4) == size);
    }
    fclose(file);
    info.frames = 30; assert(MP4_Header(header, &info));
    MP4_Info_t parsed; assert(MP4_Parse(header, &parsed) && parsed.frames == 30);
    rewind(mp4); assert(fwrite(header, 1, sizeof(header), mp4) == sizeof(header)); fclose(mp4);
    /* High-entropy frames stress the encoder's bounded scratch/output memory. */
    assert(H264_Begin(w, h, 25));
    uint32_t random = 1;
    for (unsigned frame = 0; frame < 3; ++frame)
    {
      for (unsigned n = 0; n < w * h * 2; ++n)
      { random = random * 1664525U + 1013904223U; MEDIA_CAPTURE_BUFFER[n] = random >> 24; }
      uint8_t *data; uint32_t size;
      assert(H264_Frame(MEDIA_CAPTURE_BUFFER, NULL, &data, &size));
      assert(data >= MEDIA_CAPTURE_BUFFER && data + size <= Media_Memory + sizeof(Media_Memory));
    }
  }
  puts("PASS: H.264 encoding from shared camera/scratch memory, both resolutions");
}
