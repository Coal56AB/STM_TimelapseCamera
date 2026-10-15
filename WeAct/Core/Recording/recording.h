/** @file recording.h
 *  @brief Сеансы записи и независимые AVI-фрагменты заданной длительности.
 */
#ifndef RECORDING_H
#define RECORDING_H
#include "store.h"
#include "avi.h"

#define RECORDING_ROOT "0:/TL"
#define RECORDING_MAX_ID 9999999U
typedef struct
{
  AVI_Format_t format;
  uint32_t id, frames;
  uint32_t fragment, fragment_frames, fragment_limit;
  Store_File_t file;
  uint8_t active;
} Recording_t;

Store_Result_t Recording_Init(void);
Store_Result_t Recording_Start(Recording_t *recording, const AVI_Format_t *format, uint16_t fragment_seconds);
Store_Result_t Recording_Append(Recording_t *recording, const uint8_t *rgb565_be, const char *timestamp);
Store_Result_t Recording_Stop(Recording_t *recording);
void Recording_Abort(Recording_t *recording);
Store_Result_t Recording_Select(uint32_t current, uint32_t *next);
Store_Result_t Recording_Latest(uint32_t *id);
Store_Result_t Recording_NextFragment(uint32_t id, uint32_t after, uint32_t *next);
void Recording_Path(char path[40], uint32_t id, uint32_t frame, const char *extension);

/** Последовательное виртуальное AVI для объединения на SD и передачи по USB. */
typedef struct
{
  AVI_Format_t format;
  Store_File_t source;
  uint32_t id, frames, opened_fragment;
  uint64_t size, position;
  uint8_t extended;
  uint32_t fragments, source_frames, source_first;
  uint8_t header[AVI_HEADER_SIZE];
} Recording_Stream_t;
Store_Result_t Recording_StreamOpen(Recording_Stream_t *stream, uint32_t id);
Store_Result_t Recording_StreamRead(Recording_Stream_t *stream, void *data,
                                    uint32_t capacity, uint32_t *read);
void Recording_StreamClose(Recording_Stream_t *stream);

typedef struct
{
  Recording_Stream_t stream;
  Store_File_t output;
  uint8_t buffer[4096];
  uint8_t active;
} Recording_Merge_t;
Store_Result_t Recording_MergeStart(Recording_Merge_t *merge, uint32_t id);
Store_Result_t Recording_MergeProcess(Recording_Merge_t *merge);
void Recording_MergeCancel(Recording_Merge_t *merge);
#endif
