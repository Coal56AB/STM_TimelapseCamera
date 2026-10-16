#include "recording.h"
#include "image_text.h"
#include "avi_compressed.h"
#include "jpeg_frame.h"
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t min, max, after, next, count; } Scan_t;
static uint32_t frame_sizes[7500] __attribute__((section(".bss.dma_buffer"), aligned(32)));

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
    if (id > scan->after && (!scan->next || id < scan->next)) scan->next = id;
  }
  return 1;
}

Store_Result_t Recording_NextFragment(uint32_t id, uint32_t after, uint32_t *next)
{
  Scan_t scan = {0};
  char path[40];
  if (!id || id > RECORDING_MAX_ID) return STORE_NOT_FOUND;
  scan.after = after;
  Recording_Path(path, id, 0, NULL);
  Store_Result_t result = Store_Scan(path, ScanFrame, &scan);
  *next = scan.next;
  return result;
}

Store_Result_t Recording_FragmentCount(uint32_t id, uint32_t *count)
{
  Scan_t scan = {0}; char path[40];
  Recording_Path(path, id, 0, NULL);
  Store_Result_t result = Store_Scan(path, ScanFrame, &scan);
  if (result != STORE_OK) return result;
  if (!scan.count) return STORE_NOT_FOUND;
  if (scan.count != scan.max) return STORE_IO;
  *count = scan.count; return STORE_OK;
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
  if (r->active || !fragment_seconds || (uint32_t)format->fps * fragment_seconds > 7500 ||
      format->codec > AVI_MJPEG || !AVI_Header(header, format, (uint32_t)format->fps * fragment_seconds) || format->width > 320 || format->height > 240) return STORE_IO;
  Store_Result_t result = Store_Scan(RECORDING_ROOT, ScanSession, &scan);
  if (result != STORE_OK) return result;
  if (scan.max == RECORDING_MAX_ID) return STORE_FULL;
  r->id = scan.max + 1;
  Recording_Path(path, r->id, 0, NULL);
  result = Store_Mkdir(path);
  if (result != STORE_OK) return result;
  r->format = *format;
  r->frames = 0; r->fragment = 1; r->fragment_frames = 0;
  r->data_bytes = r->max_frame = 0;
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
  uint32_t offset = 0;
  AVI_IndexHeader(index, r->fragment_frames);
  result = Store_Write(&r->file, index, 8);
  for (uint32_t first = 0; result == STORE_OK && first < r->fragment_frames; first += 32)
  {
    uint32_t count = r->fragment_frames - first;
    if (count > 32) count = 32;
    for (uint32_t i = 0; i < count; ++i)
    {
      if (r->format.codec == AVI_MJPEG)
      {
        uint32_t size = frame_sizes[first + i];
        AVI_CompressedIndex(index + i * 16, offset, size);
        offset += 8 + size + (size & 1);
      }
      else AVI_IndexEntry(index + i * 16, &r->format, first + i);
    }
    if (result == STORE_OK) result = Store_Write(&r->file, index, count * 16);
  }
  if (result == STORE_OK) result = Store_Seek(&r->file, 0);
  if (r->format.codec == AVI_MJPEG)
  {
    AVI_Info_t info = {r->format, r->fragment_frames, r->data_bytes, r->max_frame};
    AVI_CompressedHeader(header, &info);
  }
  else AVI_Header(header, &r->format, r->fragment_frames);
  if (result == STORE_OK) result = Store_Write(&r->file, header, sizeof(header));
  if (result == STORE_OK) result = Store_Sync(&r->file);
  Store_Result_t closed = Store_Close(&r->file);
  if (result == STORE_OK) result = closed;
  if (result != STORE_OK) return result;
  Recording_Path(tmp, r->id, r->fragment, "TMP");
  Recording_Path(final, r->id, r->fragment, "AVI");
  result = Store_Rename(tmp, final);
  if (result == STORE_OK) { ++r->fragment; r->fragment_frames = 0; r->data_bytes = r->max_frame = 0; }
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
  if (r->format.codec == AVI_MJPEG)
  {
    uint32_t size = 0; const uint8_t *jpeg;
    if (!JPEG_Frame(rgb, r->format.width, r->format.height, timestamp, &jpeg, &size))
    { result = STORE_IO; goto failed; }
    memcpy(chunk, "00dc", 4); AVI_Write32(chunk + 4, size);
    result = Store_Write(&r->file, chunk, sizeof(chunk));
    if (result == STORE_OK) result = Store_Write(&r->file, jpeg, size);
    if (result == STORE_OK && (size & 1)) { const uint8_t pad = 0; result = Store_Write(&r->file, &pad, 1); }
    if (result != STORE_OK) goto failed;
    frame_sizes[r->fragment_frames] = size;
    r->data_bytes += 8 + size + (size & 1);
    if (size > r->max_frame) r->max_frame = size;
  }
  else
  {
    AVI_ChunkHeader(chunk, &r->format);
    result = Store_Write(&r->file, chunk, sizeof(chunk));
    if (result != STORE_OK) goto failed;
  }
  if (r->format.codec == AVI_DIB) for (uint16_t y = r->format.height; y > 0; --y)
  {
    AVI_ConvertRow(row, rgb + (y - 1U) * r->format.width * 2U, r->format.width);
    ImageText_Row(row, r->format.width, r->format.height, (uint16_t)(y - 1U), timestamp);
    result = Store_Write(&r->file, row, stride);
    if (result != STORE_OK) goto failed;
  }
  result = r->format.codec == AVI_DIB || (r->fragment_frames + 1) % 25 == 0 ? Store_Sync(&r->file) : STORE_OK;
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

Store_Result_t Recording_MergeStart(Recording_Merge_t *m, uint32_t id)
{
  char path[40];
  uint64_t size, free_bytes, limit;
  Store_Result_t result;
  if (m->active) return STORE_IO;
  Recording_Path(path, id, 0, "AVI");
  result = Store_Stat64(path, &size);
  if (result == STORE_OK) return STORE_EXISTS;
  if (result != STORE_NOT_FOUND) return result;
  result = Recording_StreamOpen(&m->stream, id);
  if (result != STORE_OK) return result;
  result = Store_FileLimit(&limit);
  if (result == STORE_OK && m->stream.size > limit) result = STORE_FULL;
  if (result == STORE_OK) result = Store_Free(&free_bytes);
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
