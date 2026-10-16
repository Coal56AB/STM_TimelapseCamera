#ifndef DOWNLOAD_H
#define DOWNLOAD_H
#include "recording.h"
#define DOWNLOAD_BLOCK_SIZE 16384U
typedef struct
{
  Recording_Stream_t stream;
  Store_File_t file;
  uint64_t size, position;
  uint32_t token, touched;
  uint8_t active, merged;
} Download_t;
void Download_Close(Download_t *download);
void Download_Process(Download_t *download, uint8_t enabled, uint32_t now);
/* FILES request -> FILE response header and optional binary payload. */
void Download_Command(Download_t *download, const char *request, uint32_t now, uint8_t enabled,
                      char header[96], uint8_t data[DOWNLOAD_BLOCK_SIZE], uint32_t *length);
#endif
