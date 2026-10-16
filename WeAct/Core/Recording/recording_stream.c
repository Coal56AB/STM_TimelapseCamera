#include "recording.h"
#include "avi_extended.h"
#include "avi_compressed.h"
#include <string.h>

static Store_Result_t OpenSource(Recording_Stream_t *s, uint32_t fragment)
{
  uint32_t size, count;
  uint8_t header[MP4_HEADER_SIZE], actual[16], expected[16];
  AVI_Info_t info;
  Store_Result_t result;
  if (s->opened_fragment == fragment) return STORE_OK;
  result = Store_Close(&s->source);
  s->opened_fragment = 0;
  if (result != STORE_OK) return result;
  result = Recording_OpenFragment(&s->source, s->id, fragment, &size);
  if (result != STORE_OK) return result;
  if (Store_Read(&s->source, header, AVI_HEADER_SIZE) != STORE_OK) return STORE_IO;
  if (!memcmp(header + 4, "ftyp", 4))
  {
    MP4_Info_t mp4;
    if (size <= MP4_HEADER_SIZE || Store_Read(&s->source, header + AVI_HEADER_SIZE, MP4_HEADER_SIZE - AVI_HEADER_SIZE) != STORE_OK ||
        !MP4_Parse(header, &mp4) || (uint64_t)mp4.frames * (MP4_PACKET_SIZE + 5) > size - MP4_HEADER_SIZE) return STORE_IO;
    if (fragment == 1) { s->format = mp4.format; s->mp4 = mp4; }
    if (memcmp(&s->format, &mp4.format, sizeof(mp4.format)) || s->mp4.sps_size != mp4.sps_size || s->mp4.pps_size != mp4.pps_size ||
        memcmp(s->mp4.sps, mp4.sps, mp4.sps_size) || memcmp(s->mp4.pps, mp4.pps, mp4.pps_size)) return STORE_IO;
    s->source_bytes = size - MP4_HEADER_SIZE; s->source_frames = mp4.frames;
    s->opened_fragment = fragment; s->source_position = s->mp4_frame = 0;
    return STORE_OK;
  }
  if (
      !AVI_Info(header, &info) || size != (info.format.codec == AVI_DIB ? AVI_Size(&info.format, info.frames) : AVI_CompressedSize(&info))) return STORE_IO;
  AVI_Format_t format = info.format; count = info.frames;
  if (format.width > 320 || format.height > 240) return STORE_IO;
  if (fragment == 1) s->format = format;
  if (format.width != s->format.width || format.height != s->format.height || format.fps != s->format.fps || format.codec != s->format.codec) return STORE_IO;
  if (format.codec == AVI_MJPEG)
  {
    if (Store_Read(&s->source, actual, 10) != STORE_OK || memcmp(actual, "00dc", 4) || (actual[8] != 0xff || actual[9] != 0xd8) ||
        !AVI_Read32(actual + 4) || AVI_Read32(actual + 4) > info.max_frame) return STORE_IO;
    if (Store_Seek(&s->source, AVI_HEADER_SIZE + info.data_bytes) != STORE_OK ||
        Store_Read(&s->source, actual, 8) != STORE_OK || memcmp(actual, "idx1", 4) || AVI_Read32(actual + 4) != count * 16U) return STORE_IO;
    if (Store_Read(&s->source, actual, 16) != STORE_OK || memcmp(actual, "00dc", 4) ||
        AVI_Read32(actual + 8) != 4 || !(AVI_Read32(actual + 4) & 0x10)) return STORE_IO;
    if (Store_Seek(&s->source, size - 16) != STORE_OK || Store_Read(&s->source, actual, 16) != STORE_OK ||
        memcmp(actual, "00dc", 4) || !AVI_Read32(actual + 12) || AVI_Read32(actual + 12) > info.max_frame ||
        AVI_Read32(actual + 8) + 4ULL + AVI_Read32(actual + 12) + (AVI_Read32(actual + 12) & 1) != info.data_bytes) return STORE_IO;
    s->source_bytes = info.data_bytes;
    if (info.max_frame > s->max_frame) s->max_frame = info.max_frame;
    s->opened_fragment = fragment; s->source_frames = count;
    return STORE_OK;
  }
  AVI_ChunkHeader(expected, &format);
  if (Store_Read(&s->source, actual, 8) != STORE_OK || memcmp(actual, expected, 8)) return STORE_IO;
  if (Store_Seek(&s->source, AVI_HEADER_SIZE + count * (8 + AVI_FrameBytes(&format))) != STORE_OK) return STORE_IO;
  AVI_IndexHeader(expected, count);
  if (Store_Read(&s->source, actual, 8) != STORE_OK || memcmp(actual, expected, 8)) return STORE_IO;
  /* Проверяем крайние записи индекса, промежуточные chunk headers — при чтении. */
  AVI_IndexEntry(expected, &format, 0);
  if (Store_Read(&s->source, actual, 16) != STORE_OK || memcmp(actual, expected, 16)) return STORE_IO;
  if (Store_Seek(&s->source, size - 16) != STORE_OK) return STORE_IO;
  AVI_IndexEntry(expected, &format, count - 1);
  if (Store_Read(&s->source, actual, 16) != STORE_OK || memcmp(actual, expected, 16)) return STORE_IO;
  s->opened_fragment = fragment;
  s->source_frames = count;
  return STORE_OK;
}

Store_Result_t Recording_StreamOpen(Recording_Stream_t *s, uint32_t id)
{
  Store_Result_t result;
  memset(s, 0, sizeof(*s));
  if (!id || id > RECORDING_MAX_ID) return STORE_NOT_FOUND;
  s->id = id;
  result = Recording_FragmentCount(id, &s->fragments);
  if (result != STORE_OK) return result;
  if (!s->fragments) return STORE_NOT_FOUND;
  for (uint32_t i = 1; i <= s->fragments; ++i)
  {
    result = OpenSource(s, i);
    if (result != STORE_OK) { Recording_StreamClose(s); return result; }
    if (s->source_frames > UINT32_MAX - s->frames) { Recording_StreamClose(s); return STORE_FULL; }
    s->frames += s->source_frames;
    if (s->format.codec == AVI_H264) s->size += s->source_bytes;
    if (s->format.codec == AVI_MJPEG)
    {
      s->size += s->source_bytes + 32ULL + 8ULL * s->source_frames;
      if (i == 1) { s->segment_bytes = s->source_bytes; s->segment_frames = s->source_frames; }
    }
  }
  if (s->format.codec == AVI_H264)
  {
    s->size += MP4_HEADER_SIZE; s->mp4.frames = s->frames;
    if (!MP4_Header(s->header, &s->mp4)) { Recording_StreamClose(s); return STORE_IO; }
    s->segment = 1;
  }
  else if (s->format.codec == AVI_MJPEG)
  {
    s->size += 524ULL + 16ULL * s->fragments + 24ULL * (s->fragments - 1);
    s->segment = 1; s->extended = 1;
  }
  else
  {
    s->size = AVI_Size(&s->format, s->frames);
    s->extended = !s->size;
    if (s->size) AVI_Header(s->header, &s->format, s->frames);
    else s->size = AVI_ExtendedSize(&s->format, s->frames);
  }
  Store_Close(&s->source); s->opened_fragment = 0; s->source_frames = 0; s->source_first = 0;
  return STORE_OK;
}

/* One OpenDML segment per source fragment; copy its compressed movi bytes unchanged. */
static Store_Result_t CompressedRead(Recording_Stream_t *s, uint8_t *out, uint32_t capacity, uint32_t *read)
{
  uint32_t header = 524U + s->fragments * 16U;
  *read = 0;
  while (capacity && s->position < s->size)
  {
    uint8_t small[512] = {0};
    uint32_t at = 0, size = 0, available;
    uint8_t from_file = 0;
    Store_Result_t result;
    if (s->position < header)
    {
      uint32_t p = (uint32_t)s->position;
      if (p < 212)
      {
        AVI_Info_t info = {s->format, s->segment_frames, s->segment_bytes, s->max_frame};
        AVI_CompressedHeader(small, &info);
        AVI_Write32(small + 4, header + info.data_bytes + 32 + 8 * info.frames - 8);
        AVI_Write32(small + 16, header - 32); AVI_Write32(small + 92, 148 + 16 * s->fragments);
        AVI_Write32(small + 140, s->frames);
        at = p; size = 212;
      }
      else if (p < 244)
      {
        memcpy(small, "indx", 4); AVI_Write32(small + 4, 24 + 16 * s->fragments);
        small[8] = 4; AVI_Write32(small + 12, s->fragments); memcpy(small + 16, "00dc", 4);
        at = p - 212; size = 32;
      }
      else if (p < 244 + 16 * s->fragments)
      {
        uint32_t part = (p - 244) / 16 + 1;
        if (s->catalog_part != part)
        {
          if (part != s->catalog_part + 1) return STORE_IO;
          if (s->catalog_part)
            s->catalog_start += (s->catalog_part == 1 ? header : 24) + s->source_bytes + 32ULL + 8ULL * s->source_frames;
          result = OpenSource(s, part); if (result != STORE_OK) return result;
          s->catalog_part = part;
        }
        uint64_t index = s->catalog_start + (part == 1 ? header : 24) + s->source_bytes;
        AVI_Write32(small, (uint32_t)index); AVI_Write32(small + 4, (uint32_t)(index >> 32));
        AVI_Write32(small + 8, 32 + s->source_frames * 8); AVI_Write32(small + 12, s->source_frames);
        at = (p - 244) % 16; size = 16;
      }
      else if (p < header - 12)
      {
        uint32_t relative = p - (244 + s->fragments * 16);
        if (relative < 24)
        {
          memcpy(small, "LIST", 4); AVI_Write32(small + 4, 260); memcpy(small + 8, "odmldmlh", 8);
          AVI_Write32(small + 16, 248); AVI_Write32(small + 20, s->frames);
          at = relative; size = 24;
        }
        else size = 268 - relative;
      }
      else
      {
        memcpy(small, "LIST", 4); AVI_Write32(small + 4, 4 + s->segment_bytes); memcpy(small + 8, "movi", 4);
        at = p - (header - 12); size = 12;
      }
    }
    else
    {
      uint32_t head = s->segment == 1 ? header : 24;
      uint64_t span = head + s->segment_bytes + 32ULL + 8ULL * s->segment_frames;
      if (s->position == s->segment_start + span)
      {
        s->segment_start += span; ++s->segment;
        result = OpenSource(s, s->segment); if (result != STORE_OK) return result;
        s->segment_bytes = s->source_bytes; s->segment_frames = s->source_frames;
        head = 24;
      }
      uint32_t local = (uint32_t)(s->position - s->segment_start);
      if (local < head)
      {
        memcpy(small, "RIFF", 4); AVI_Write32(small + 4, 24 + s->segment_bytes + 32 + s->segment_frames * 8 - 8);
        memcpy(small + 8, "AVIXLIST", 8); AVI_Write32(small + 16, 4 + s->segment_bytes); memcpy(small + 20, "movi", 4);
        at = local; size = 24;
      }
      else if (local < head + s->segment_bytes)
      {
        result = OpenSource(s, s->segment); if (result != STORE_OK) return result;
        result = Store_Seek(&s->source, AVI_HEADER_SIZE + local - head); if (result != STORE_OK) return result;
        from_file = 1; size = head + s->segment_bytes - local;
      }
      else if (local < head + s->segment_bytes + 32)
      {
        memcpy(small, "ix00", 4); AVI_Write32(small + 4, 24 + s->segment_frames * 8);
        small[8] = 2; small[11] = 1; AVI_Write32(small + 12, s->segment_frames); memcpy(small + 16, "00dc", 4);
        uint64_t base = s->segment_start + head;
        AVI_Write32(small + 20, (uint32_t)base); AVI_Write32(small + 24, (uint32_t)(base >> 32));
        at = local - head - s->segment_bytes; size = 32;
      }
      else
      {
        uint32_t relative = local - head - s->segment_bytes - 32, entry = relative / 8;
        uint32_t count = s->segment_frames - entry; if (count > 32) count = 32;
        result = OpenSource(s, s->segment); if (result != STORE_OK) return result;
        result = Store_Seek(&s->source, AVI_HEADER_SIZE + s->source_bytes + 8 + entry * 16);
        if (result != STORE_OK || Store_Read(&s->source, small, count * 16) != STORE_OK) return STORE_IO;
        for (unsigned i = 0; i < count; ++i)
        {
          uint32_t offset = AVI_Read32(small + i * 16 + 8), bytes = AVI_Read32(small + i * 16 + 12);
          if (memcmp(small + i * 16, "00dc", 4) || !(AVI_Read32(small + i * 16 + 4) & 0x10) ||
              offset < 4 || !bytes || bytes > s->max_frame || offset + 4ULL + bytes + (bytes & 1) > s->source_bytes) return STORE_IO;
          AVI_Write32(small + i * 8, offset + 4); AVI_Write32(small + i * 8 + 4, bytes);
        }
        at = relative % 8; size = count * 8;
      }
    }
    available = size - at; if (available > capacity) available = capacity;
    if (!available) return STORE_IO;
    if (from_file)
    { result = Store_Read(&s->source, out, available); if (result != STORE_OK) return result; }
    else memcpy(out, small + at, available);
    out += available; capacity -= available; *read += available; s->position += available;
  }
  return STORE_OK;
}

static Store_Result_t Mp4Read(Recording_Stream_t *s, uint8_t *out, uint32_t capacity, uint32_t *read)
{
  *read = 0;
  while (capacity && s->position < s->size)
  {
    const uint8_t *memory = NULL; uint32_t available;
    if (s->position < MP4_HEADER_SIZE)
    { memory = s->header + (uint32_t)s->position; available = MP4_HEADER_SIZE - (uint32_t)s->position; }
    else
    {
      Store_Result_t result = OpenSource(s, s->segment); if (result != STORE_OK) return result;
      if (!s->packet_left)
      {
        if (s->mp4_frame == s->source_frames)
        {
          if (s->source_position != s->source_bytes || s->segment >= s->fragments) return STORE_IO;
          s->source_first += s->source_frames;
          result = OpenSource(s, ++s->segment); if (result != STORE_OK) return result;
        }
        uint8_t key; uint32_t bytes;
        if (s->source_bytes - s->source_position < MP4_PACKET_SIZE ||
            Store_Read(&s->source, s->packet, sizeof(s->packet)) != STORE_OK ||
            !MP4_ParsePacket(s->packet, s->mp4_frame, &bytes, &key) ||
            bytes > s->source_bytes - s->source_position - MP4_PACKET_SIZE || (!s->mp4_frame && !key)) return STORE_IO;
        MP4_Packet(s->packet, s->source_first + s->mp4_frame, bytes, key);
        s->packet_at = 0; s->packet_left = MP4_PACKET_SIZE + bytes;
        ++s->mp4_frame;
      }
      if (s->packet_at < MP4_PACKET_SIZE)
      { memory = s->packet + s->packet_at; available = MP4_PACKET_SIZE - s->packet_at; }
      else available = s->packet_left;
    }
    if (available > capacity) available = capacity;
    if (memory) memcpy(out, memory, available);
    else if (Store_Read(&s->source, out, available) != STORE_OK) return STORE_IO;
    if (s->position >= MP4_HEADER_SIZE)
    { s->packet_at += available; s->packet_left -= available; s->source_position += available; }
    out += available; *read += available; capacity -= available; s->position += available;
  }
  if (s->position == s->size && (s->packet_left || s->mp4_frame != s->source_frames || s->segment != s->fragments)) return STORE_IO;
  return STORE_OK;
}

Store_Result_t Recording_StreamRead(Recording_Stream_t *s, void *data, uint32_t capacity, uint32_t *read)
{
  uint8_t *out = data, small[224];
  uint32_t chunk = AVI_FrameBytes(&s->format) + 8;
  uint64_t index_at = AVI_HEADER_SIZE + (uint64_t)chunk * s->frames;
  *read = 0;
  if (!s->size) return STORE_IO;
  if (s->format.codec == AVI_H264) return Mp4Read(s, data, capacity, read);
  if (s->format.codec == AVI_MJPEG) return CompressedRead(s, data, capacity, read);
  while (capacity && s->position < s->size)
  {
    uint32_t available, offset = 0, frame = 0;
    const uint8_t *memory = NULL;
    if (s->extended)
    {
      if (AVI_ExtendedRegion(&s->format, s->frames, s->position, small, &available, &frame, &offset)) memory = small;
    }
    else if (s->position < AVI_HEADER_SIZE)
    {
      available = AVI_HEADER_SIZE - (uint32_t)s->position;
      memory = s->header + (uint32_t)s->position;
    }
    else if (s->position < index_at)
    {
      frame = (uint32_t)((s->position - AVI_HEADER_SIZE) / chunk);
      offset = (uint32_t)((s->position - AVI_HEADER_SIZE) % chunk);
      available = chunk - offset;
    }
    else if (s->position < index_at + 8)
    {
      AVI_IndexHeader(small, s->frames);
      offset = (uint32_t)(s->position - index_at);
      memory = small + offset; available = 8 - offset;
    }
    else
    {
      offset = (uint32_t)(s->position - index_at - 8);
      AVI_IndexEntry(small, &s->format, offset / 16);
      memory = small + offset % 16; available = 16 - offset % 16;
    }
    if (!memory)
    {
      Store_Result_t result;
      if (!s->opened_fragment)
      {
        result = OpenSource(s, 1);
        if (result != STORE_OK) return result;
      }
      while (frame >= s->source_first + s->source_frames)
      {
        s->source_first += s->source_frames;
        result = OpenSource(s, s->opened_fragment + 1);
        if (result != STORE_OK) return result;
      }
      uint32_t source_offset = AVI_HEADER_SIZE + (frame - s->source_first) * chunk;
      if (offset == 0)
      {
        uint8_t actual[8]; AVI_ChunkHeader(small, &s->format);
        if (Store_Seek(&s->source, source_offset) != STORE_OK ||
            Store_Read(&s->source, actual, 8) != STORE_OK || memcmp(actual, small, 8)) return STORE_IO;
        /* Already read and checked: emit the header without seeking back over it. */
        memory = small; available = 8;
      }
      else
      {
        result = Store_Seek(&s->source, source_offset + offset);
        if (result != STORE_OK) return result;
      }
    }
    if (available > capacity) available = capacity;
    if (memory) memcpy(out, memory, available);
    else
    {
      Store_Result_t result = Store_Read(&s->source, out, available);
      if (result != STORE_OK) return result;
    }
    out += available; capacity -= available; *read += available; s->position += available;
  }
  return STORE_OK;
}

void Recording_StreamClose(Recording_Stream_t *s)
{
  Store_Close(&s->source);
  s->opened_fragment = 0;
  s->size = 0;
}
