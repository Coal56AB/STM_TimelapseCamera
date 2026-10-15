#ifndef TIMELAPSE_CONFIG_H
#define TIMELAPSE_CONFIG_H
#include "store.h"
#include <stdint.h>
#define TIMELAPSE_INTERVAL_COUNT 8U
#define TIMELAPSE_RESOLUTION_COUNT 2U
#define TIMELAPSE_PLAYBACK_FPS 25U
#define TIMELAPSE_FRAGMENT_COUNT 4U
typedef struct { uint8_t interval, resolution; uint32_t generation; uint8_t fragment; } Timelapse_Config_t;
extern const uint16_t Timelapse_Fragments[TIMELAPSE_FRAGMENT_COUNT];
extern const uint32_t Timelapse_Intervals[TIMELAPSE_INTERVAL_COUNT];
extern const uint16_t Timelapse_Resolutions[TIMELAPSE_RESOLUTION_COUNT][2];
Store_Result_t Timelapse_ConfigLoad(Timelapse_Config_t *config);
Store_Result_t Timelapse_ConfigSave(Timelapse_Config_t *config);
#endif
