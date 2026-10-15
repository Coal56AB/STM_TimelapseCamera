#include "download.h"
#include <stdio.h>
#include <string.h>

void Download_Close(Download_t *d)
{
  Store_Close(&d->file); Recording_StreamClose(&d->stream);
  d->active = 0; d->size = d->position = 0;
}
void Download_Process(Download_t *d, uint8_t enabled, uint32_t now)
{
  if (!enabled || (d->active && now - d->touched >= 10000)) Download_Close(d);
}
static Store_Result_t Fragment(uint32_t id, uint32_t part, Store_File_t *file, uint32_t *size,
                               AVI_Format_t *format, uint32_t *frames)
{
  uint8_t header[AVI_HEADER_SIZE]; char path[40];
  if (!id || id > RECORDING_MAX_ID || !part || part > RECORDING_MAX_ID) return STORE_NOT_FOUND;
  Recording_Path(path, id, part, "AVI");
  Store_Result_t result = Store_Stat(path, size);
  if (result == STORE_OK) result = Store_Open(file, path, STORE_READ);
  if (result == STORE_OK) result = Store_Read(file, header, sizeof(header));
  if (result == STORE_OK && (!AVI_Parse(header, format, frames) || *size != AVI_Size(format, *frames))) result = STORE_IO;
  if (result == STORE_OK) result = Store_Seek(file, 0);
  return result;
}

void Download_Command(Download_t *d, const char *request, uint32_t now, uint8_t enabled,
                      char header[96], uint8_t data[DOWNLOAD_BLOCK_SIZE], uint32_t *length)
{
  unsigned long seq = 0, a = 0, b = 0;
  unsigned long long offset = 0;
  char operation[12] = {0}, extra;
  Store_Result_t result = STORE_IO;
  *length = 0;
  if (sscanf(request, "FILES %lu %11s", &seq, operation) != 2) goto reply;
  if (!enabled) { Download_Close(d); goto reply; }
  if (!strcmp(operation, "LIST") && sscanf(request, "FILES %lu LIST %lu %c", &seq, &a, &extra) == 2 && a <= RECORDING_MAX_ID)
  {
    uint32_t id = 0;
    Download_Close(d);
    result = Recording_Select((uint32_t)a, &id);
    if (result == STORE_NOT_FOUND || (result == STORE_OK && id <= a))
    { result = STORE_OK; memcpy(data, "null", 4); *length = 4; }
    else if (result == STORE_OK)
    {
      Store_Result_t info = Recording_StreamOpen(&d->stream, id);
      *length = (uint32_t)snprintf((char *)data, DOWNLOAD_BLOCK_SIZE,
        "{\"id\":%lu,\"frames\":%lu,\"fragments\":%lu,\"fps\":%u,\"width\":%u,\"height\":%u,\"size\":%llu,\"error\":%u}",
        (unsigned long)id, (unsigned long)d->stream.frames, (unsigned long)d->stream.fragments,
        d->stream.format.fps, d->stream.format.width, d->stream.format.height,
        (unsigned long long)d->stream.size, (unsigned)info);
      Recording_StreamClose(&d->stream);
    }
  }
  else if (!strcmp(operation, "PARTS") && sscanf(request, "FILES %lu PARTS %lu %lu %c", &seq, &a, &b, &extra) == 3 && a <= RECORDING_MAX_ID && b <= RECORDING_MAX_ID)
  {
    uint32_t next = 0, size = 0, frames = 0; AVI_Format_t format;
    Download_Close(d);
    result = Recording_NextFragment((uint32_t)a, (uint32_t)b, &next);
    if (result == STORE_OK && !next) { memcpy(data, "null", 4); *length = 4; }
    else if (result == STORE_OK)
    {
      result = Fragment((uint32_t)a, next, &d->file, &size, &format, &frames);
      if (result == STORE_OK) *length = (uint32_t)snprintf((char *)data, DOWNLOAD_BLOCK_SIZE,
        "{\"id\":%lu,\"frames\":%lu,\"fps\":%u,\"size\":%lu}",
        (unsigned long)next, (unsigned long)frames, format.fps, (unsigned long)size);
      Store_Close(&d->file);
    }
  }
  else if (!strcmp(operation, "OPEN") && sscanf(request, "FILES %lu OPEN %lu %lu %c", &seq, &a, &b, &extra) == 3 && a <= RECORDING_MAX_ID && b <= RECORDING_MAX_ID)
  {
    Download_Close(d);
    if (!b)
    {
      result = Recording_StreamOpen(&d->stream, (uint32_t)a);
      d->size = d->stream.size; d->merged = 1;
    }
    else
    {
      uint32_t size = 0, frames; AVI_Format_t format;
      result = Fragment((uint32_t)a, (uint32_t)b, &d->file, &size, &format, &frames);
      d->size = size; d->merged = 0;
    }
    if (result == STORE_OK)
    {
      if (++d->token == 0) ++d->token;
      d->active = 1; d->touched = now;
    }
    else Download_Close(d);
  }
  else if (!strcmp(operation, "READ") && sscanf(request, "FILES %lu READ %lu %llu %c", &seq, &a, &offset, &extra) == 3 &&
           d->active && a == d->token && offset == d->position)
  {
    d->touched = now;
    uint32_t count = d->size - d->position < DOWNLOAD_BLOCK_SIZE ? (uint32_t)(d->size - d->position) : DOWNLOAD_BLOCK_SIZE;
    if (d->merged) result = Recording_StreamRead(&d->stream, data, count, length);
    else { result = Store_Read(&d->file, data, count); if (result == STORE_OK) *length = count; }
    if (result == STORE_OK) d->position += *length;
    else { *length = 0; Download_Close(d); }
  }
  else if (!strcmp(operation, "CLOSE") && sscanf(request, "FILES %lu CLOSE %lu %c", &seq, &a, &extra) == 2 && a == d->token)
  { Download_Close(d); result = STORE_OK; }
reply:
  snprintf(header, 96, "FILE %lu %u %lu %llu %lu\n", seq, (unsigned)result,
           (unsigned long)d->token, (unsigned long long)d->size, (unsigned long)*length);
}
