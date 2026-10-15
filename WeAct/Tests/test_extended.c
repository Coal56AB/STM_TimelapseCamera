#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "avi_extended.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
  assert(argc == 2);
  AVI_Format_t format = {320, 240, 25};
  const uint32_t frames = 30000;
  uint64_t size = AVI_ExtendedSize(&format, frames), position = 0;
  assert(size > UINT32_MAX);
  FILE *file = fopen(argv[1], "wb"); assert(file);
  uint8_t bytes[224]; uint32_t frame_count = 0;
  while (position < size)
  {
    uint32_t length, frame, offset;
    int memory = AVI_ExtendedRegion(&format, frames, position, bytes, &length, &frame, &offset);
    assert(length && position + length <= size);
    if (!memory)
    {
      assert(frame == frame_count++ && offset == 0 && length == AVI_FrameBytes(&format) + 8);
      AVI_ChunkHeader(bytes, &format);
      assert(fwrite(bytes, 1, 8, file) == 8);
      assert(fseeko(file, length - 8, SEEK_CUR) == 0);
    }
    else
    {
      /* Partial reads of every generated metadata region must reproduce its bytes. */
      for (uint32_t at = 0; at < length; ++at)
      {
        uint8_t part[224]; uint32_t n, f, o;
        assert(AVI_ExtendedRegion(&format, frames, position + at, part, &n, &f, &o));
        assert(n >= length - at && !memcmp(part, bytes + at, length - at));
      }
      assert(fwrite(bytes, 1, length, file) == length);
    }
    position += length;
  }
  assert(frame_count == frames && fclose(file) == 0);
  puts("PASS: OpenDML >4 GiB, all frame regions and partial metadata reads");
}
