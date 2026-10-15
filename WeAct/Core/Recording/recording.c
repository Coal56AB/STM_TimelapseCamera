#include "recording.h"
#include "image_text.h"
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t min, max, after, next, count; } Scan_t;

static uint32_t ParseId(const char *name, char prefix, const char *suffix)
{
  uint32_t id = 0;
  if (strlen(name) != 8U + strlen(suffix) || name[0] != prefix || strcmp(name + 8, suffix)) return 0;
  for (unsigned i = 1; i < 8; ++i)
  {
    if (name[i] < '0' || name[i] > '9') return 0;
    id = id * 10U + (uint32_t)(name[i] - '0');
  }
  return id;
}

static int ScanSession(void *context, const char *name, uint8_t directory)
{
  Scan_t *scan = context;
  uint32_t id = directory ? ParseId(name, 'R', "") : 0;
  if (!id) return 1;
  if (!scan->min || id < scan->min) scan->min = id;
  if (id > scan->max) scan->max = id;
  if (id > scan->after && (!scan->next || id < scan->next)) scan->next = id;
  return 1;
}

static int ScanFrame(void *context, const char *name, uint8_t directory)
{
  Scan_t *scan = context;
  uint32_t id = directory ? 0 : ParseId(name, 'F', ".AVI");
  if (id)
  {
    ++scan->count;
    if (id > scan->max) scan->max = id;
  }
  return 1;
}

void Recording_Path(char path[40], uint32_t id, uint32_t frame, const char *extension)
{
  if (frame) snprintf(path, 40, RECORDING_ROOT "/R%07lu/F%07lu.%s",
                      (unsigned long)id, (unsigned long)frame, extension);
  else if (extension) snprintf(path, 40, RECORDING_ROOT "/R%07lu/ALL.%s", (unsigned long)id, extension);
  else snprintf(path, 40, RECORDING_ROOT "/R%07lu", (unsigned long)id);
}

Store_Result_t Recording_Init(void)
{
  Store_Result_t result = Store_Mkdir(RECORDING_ROOT);
  return result == STORE_EXISTS ? STORE_OK : result;
}

Store_Result_t Recording_Select(uint32_t current, uint32_t *next)
{
  Scan_t scan = {0};
  scan.after = current;
  Store_Result_t result = Store_Scan(RECORDING_ROOT, ScanSession, &scan);
  if (result != STORE_OK) return result;
  *next = scan.next ? scan.next : scan.min;
  return *next ? STORE_OK : STORE_NOT_FOUND;
}

Store_Result_t Recording_Latest(uint32_t *id)
{
  Scan_t scan = {0};
  Store_Result_t result = Store_Scan(RECORDING_ROOT, ScanSession, &scan);
  if (result != STORE_OK) return result;
  *id = scan.max;
  return *id ? STORE_OK : STORE_NOT_FOUND;
}

Store_Result_t Recording_Start(Recording_t *r, const AVI_Format_t *format, uint16_t fragment_seconds)
{
  Scan_t scan = {0};
  char path[40];
  uint8_t header[AVI_HEADER_SIZE];
  if (r->active || !fragment_seconds || !AVI_Header(header, format, (uint32_t)format->fps * fragment_seconds) || format->width > 320 || format->height > 240) return STORE_IO;
  Store_Result_t result = Store_Scan(RECORDING_ROOT, ScanSession, &scan);
  if (result != STORE_OK) return result;
  if (scan.max == RECORDING_MAX_ID) return STORE_FULL;
  r->id = scan.max + 1;
  Recording_Path(path, r->id, 0, NULL);
  result = Store_Mkdir(path);
  if (result != STORE_OK) return result;
  r->format = *format;
  r->frames = 0; r->fragment = 1; r->fragment_frames = 0;
  r->fragment_limit = (uint32_t)format->fps * fragment_seconds;
  r->file = (Store_File_t){0};
  r->active = 1;
  return STORE_OK;
}

static Store_Result_t FinishFragment(Recording_t *r)
{
  if (!r->file.handle) return STORE_OK;
  uint8_t header[AVI_HEADER_SIZE], index[16 * 32];
  char tmp[40], final[40];
  Store_Result_t result;
  AVI_IndexHeader(index, r->fragment_frames);
  result = Store_Write(&r->file, index, 8);
  for (uint32_t first = 0; result == STORE_OK && first < r->fragment_frames; first += 32)
  {
    uint32_t count = r->fragment_frames - first;
    if (count > 32) count = 32;
    for (uint32_t i = 0; i < count; ++i) AVI_IndexEntry(index + i * 16, &r->format, first + i);
    result = Store_Write(&r->file, index, count * 16);
  }
  if (result == STORE_OK) result = Store_Seek(&r->file, 0);
  AVI_Header(header, &r->format, r->fragment_frames);
  if (result == STORE_OK) result = Store_Write(&r->file, header, sizeof(header));
  if (result == STORE_OK) result = Store_Sync(&r->file);
  Store_Result_t closed = Store_Close(&r->file);
  if (result == STORE_OK) result = closed;
  if (result != STORE_OK) return result;
  Recording_Path(tmp, r->id, r->fragment, "TMP");
  Recording_Path(final, r->id, r->fragment, "AVI");
  result = Store_Rename(tmp, final);
  if (result == STORE_OK) { ++r->fragment; r->fragment_frames = 0; }
  return result;
}

Store_Result_t Recording_Append(Recording_t *r, const uint8_t *rgb, const char *timestamp)
{
  char path[40];
  uint8_t header[AVI_HEADER_SIZE], row[960], chunk[8];
  Store_Result_t result;
  uint32_t stride = (r->format.width * 3U + 3U) & ~3U;
  if (!r->active || !rgb || r->fragment > RECORDING_MAX_ID || r->frames == UINT32_MAX) return STORE_IO;
  if (!r->file.handle)
  {
    Recording_Path(path, r->id, r->fragment, "TMP");
    result = Store_Open(&r->file, path, STORE_CREATE);
    if (result != STORE_OK) goto failed;
    AVI_Header(header, &r->format, 1);
    result = Store_Write(&r->file, header, sizeof(header));
    if (result != STORE_OK) goto failed;
  }
  AVI_ChunkHeader(chunk, &r->format);
  result = Store_Write(&r->file, chunk, sizeof(chunk));
  if (result != STORE_OK) goto failed;
  /* DIB: строки снизу вверх; кадр DCMI: RGB565 MSB first сверху вниз. */
  for (uint16_t y = r->format.height; y > 0; --y)
  {
    AVI_ConvertRow(row, rgb + (y - 1U) * r->format.width * 2U, r->format.width);
    ImageText_Row(row, r->format.width, r->format.height, (uint16_t)(y - 1U), timestamp);
    result = Store_Write(&r->file, row, stride);
    if (result != STORE_OK) goto failed;
  }
  result = Store_Sync(&r->file);
  if (result != STORE_OK) goto failed;
  ++r->frames; ++r->fragment_frames;
  if (r->fragment_frames == r->fragment_limit)
  {
    result = FinishFragment(r);
    if (result != STORE_OK) goto failed;
  }
  return STORE_OK;
failed:
  Store_Close(&r->file);
  r->active = 0;
  return result;
}

Store_Result_t Recording_Stop(Recording_t *r)
{
  Store_Result_t result = r->active ? FinishFragment(r) : STORE_OK;
  r->active = 0;
  return result;
}
void Recording_Abort(Recording_t *r)
{
  /* После Store_Reset старый дескриптор нельзя закрывать с записью на новую карту. */
  r->file = (Store_File_t){0};
  r->active = 0;
}

static Store_Result_t OpenSource(Recording_Stream_t *s, uint32_t fragment)
{
  char path[40];
  uint32_t size, count;
  uint8_t header[AVI_HEADER_SIZE], actual[16], expected[16];
  AVI_Format_t format;
  Store_Result_t result;
  if (s->opened_fragment == fragment) return STORE_OK;
  result = Store_Close(&s->source);
  s->opened_fragment = 0;
  if (result != STORE_OK) return result;
  Recording_Path(path, s->id, fragment, "AVI");
  result = Store_Stat(path, &size);
  if (result != STORE_OK) return result;
  result = Store_Open(&s->source, path, STORE_READ);
  if (result != STORE_OK) return result;
  if (Store_Read(&s->source, header, sizeof(header)) != STORE_OK ||
      !AVI_Parse(header, &format, &count) || size != AVI_Size(&format, count)) return STORE_IO;
  if (fragment == 1) s->format = format;
  if (format.width != s->format.width || format.height != s->format.height || format.fps != s->format.fps) return STORE_IO;
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
  Scan_t scan = {0};
  char path[40];
  Store_Result_t result;
  memset(s, 0, sizeof(*s));
  if (!id || id > RECORDING_MAX_ID) return STORE_NOT_FOUND;
  s->id = id;
  Recording_Path(path, id, 0, NULL);
  result = Store_Scan(path, ScanFrame, &scan);
  if (result != STORE_OK) return result;
  if (!scan.count) return STORE_NOT_FOUND;
  if (scan.count != scan.max) return STORE_IO;
  s->fragments = scan.count;
  for (uint32_t i = 1; i <= s->fragments; ++i)
  {
    result = OpenSource(s, i);
    if (result != STORE_OK) { Recording_StreamClose(s); return result; }
    if (s->source_frames > UINT32_MAX - s->frames) { Recording_StreamClose(s); return STORE_FULL; }
    s->frames += s->source_frames;
    if (!AVI_Size(&s->format, s->frames)) { Recording_StreamClose(s); return STORE_FULL; }
  }
  s->size = AVI_Size(&s->format, s->frames);
  if (!AVI_Header(s->header, &s->format, s->frames)) { Recording_StreamClose(s); return STORE_FULL; }
  Store_Close(&s->source); s->opened_fragment = 0; s->source_frames = 0; s->source_first = 0;
  return STORE_OK;
}

Store_Result_t Recording_StreamRead(Recording_Stream_t *s, void *data, uint32_t capacity, uint32_t *read)
{
  uint8_t *out = data, small[16];
  uint32_t chunk = AVI_FrameBytes(&s->format) + 8;
  uint32_t index_at = AVI_HEADER_SIZE + chunk * s->frames;
  *read = 0;
  if (!s->size) return STORE_IO;
  while (capacity && s->position < s->size)
  {
    uint32_t available, offset;
    const uint8_t *memory = NULL;
    if (s->position < AVI_HEADER_SIZE)
    {
      available = AVI_HEADER_SIZE - s->position;
      memory = s->header + s->position;
    }
    else if (s->position < index_at)
    {
      uint32_t frame = (s->position - AVI_HEADER_SIZE) / chunk;
      offset = (s->position - AVI_HEADER_SIZE) % chunk;
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
        uint8_t actual[8];
        AVI_ChunkHeader(small, &s->format);
        if (Store_Seek(&s->source, source_offset) != STORE_OK ||
            Store_Read(&s->source, actual, 8) != STORE_OK || memcmp(actual, small, 8)) return STORE_IO;
      }
      result = Store_Seek(&s->source, source_offset + offset);
      if (result != STORE_OK) return result;
      available = chunk - offset;
    }
    else if (s->position < index_at + 8)
    {
      AVI_IndexHeader(small, s->frames);
      offset = s->position - index_at;
      memory = small + offset;
      available = 8 - offset;
    }
    else
    {
      offset = s->position - index_at - 8;
      AVI_IndexEntry(small, &s->format, offset / 16);
      memory = small + offset % 16;
      available = 16 - offset % 16;
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

Store_Result_t Recording_MergeStart(Recording_Merge_t *m, uint32_t id)
{
  char path[40];
  uint32_t size;
  uint64_t free_bytes;
  Store_Result_t result;
  if (m->active) return STORE_IO;
  Recording_Path(path, id, 0, "AVI");
  result = Store_Stat(path, &size);
  if (result == STORE_OK) return STORE_EXISTS;
  if (result != STORE_NOT_FOUND) return result;
  result = Recording_StreamOpen(&m->stream, id);
  if (result != STORE_OK) return result;
  result = Store_Free(&free_bytes);
  if (result == STORE_OK && free_bytes < m->stream.size + 4096ULL) result = STORE_FULL;
  if (result != STORE_OK) { Recording_StreamClose(&m->stream); return result; }
  Recording_Path(path, id, 0, "TMP");
  /* Только временный результат нашего объединения можно заменить после сбоя. */
  result = Store_Remove(path);
  if (result != STORE_OK && result != STORE_NOT_FOUND) { Recording_StreamClose(&m->stream); return result; }
  result = Store_Open(&m->output, path, STORE_CREATE);
  if (result != STORE_OK) { Recording_StreamClose(&m->stream); return result; }
  m->active = 1;
  return STORE_OK;
}

Store_Result_t Recording_MergeProcess(Recording_Merge_t *m)
{
  char tmp[40], final[40];
  uint32_t read;
  Store_Result_t result;
  if (!m->active) return STORE_IO;
  result = Recording_StreamRead(&m->stream, m->buffer, sizeof(m->buffer), &read);
  if (result != STORE_OK) goto failed;
  result = Store_Write(&m->output, m->buffer, read);
  if (result != STORE_OK) goto failed;
  if (m->stream.position < m->stream.size) return STORE_OK;
  result = Store_Sync(&m->output);
  if (result != STORE_OK) goto failed;
  result = Store_Close(&m->output);
  if (result != STORE_OK) goto failed;
  Recording_Path(tmp, m->stream.id, 0, "TMP");
  Recording_Path(final, m->stream.id, 0, "AVI");
  result = Store_Rename(tmp, final);
  if (result != STORE_OK) goto failed;
  Recording_StreamClose(&m->stream);
  m->active = 0;
  return STORE_OK;
failed:
  Recording_MergeCancel(m);
  return result;
}

void Recording_MergeCancel(Recording_Merge_t *m)
{
  char path[40];
  if (!m->active) return;
  Store_Close(&m->output);
  Recording_StreamClose(&m->stream);
  Recording_Path(path, m->stream.id, 0, "TMP");
  Store_Remove(path);
  m->active = 0;
}
