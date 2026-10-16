/** @brief Запись и завершение независимого MP4-фрагмента. */
#ifndef RECORDING_MP4_H
#define RECORDING_MP4_H
#include "recording.h"
Store_Result_t Recording_MP4Append(Recording_t *recording, const uint8_t *rgb, const char *timestamp);
Store_Result_t Recording_MP4Finish(Recording_t *recording);
#endif
