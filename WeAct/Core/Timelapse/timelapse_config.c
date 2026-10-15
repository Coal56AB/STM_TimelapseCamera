#include "timelapse_config.h"
#include <string.h>

const uint32_t Timelapse_Intervals[] = {5, 10, 30, 60, 120, 300, 600, 1800};
const uint16_t Timelapse_Resolutions[][2] = {{160, 120}, {320, 240}};
const uint16_t Timelapse_Fragments[] = {30, 60, 120, 300};
static const char *const paths[] = {"0:/TL/SET0.BIN", "0:/TL/SET1.BIN"};

static uint32_t CRC(const uint8_t *data, unsigned length)
{
  uint32_t crc = 0xffffffffU;
  while (length--)
  {
    crc ^= *data++;
    for (unsigned i = 0; i < 8; ++i) crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
  }
  return ~crc;
}
static void Put32(uint8_t *p, uint32_t v)
{
  for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8U * i));
}
static uint32_t Get32(const uint8_t *p)
{
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
Store_Result_t Timelapse_ConfigLoad(Timelapse_Config_t *config)
{
  uint8_t data[16], found = 0;
  Store_Result_t status = STORE_OK;
  *config = (Timelapse_Config_t){0, 1, 0, 3};
  for (unsigned slot = 0; slot < 2; ++slot)
  {
    Store_File_t file = {0};
    Store_Result_t result = Store_Open(&file, paths[slot], STORE_READ);
    if (result == STORE_NOT_FOUND) continue;
    if (result != STORE_OK) { status = result; continue; }
    result = Store_Read(&file, data, sizeof(data));
    Store_Result_t closed = Store_Close(&file);
    if (result != STORE_OK || closed != STORE_OK || (memcmp(data, "TLC1", 4) && memcmp(data, "TLC2", 4)) ||
        Get32(data + 12) != CRC(data, 12) || data[8] >= TIMELAPSE_INTERVAL_COUNT ||
        data[9] >= TIMELAPSE_RESOLUTION_COUNT || (data[3] == '2' && data[10] >= TIMELAPSE_FRAGMENT_COUNT))
    { status = STORE_IO; continue; }
    uint32_t generation = Get32(data + 4);
    if (!found || (int32_t)(generation - config->generation) > 0)
      *config = (Timelapse_Config_t){data[8], data[9], generation, data[3] == '2' ? data[10] : 3};
    found = 1;
  }
  return found ? STORE_OK : status;
}
Store_Result_t Timelapse_ConfigSave(Timelapse_Config_t *config)
{
  uint8_t data[16] = {'T', 'L', 'C', '2'};
  Store_File_t file = {0};
  uint32_t generation = config->generation + 1;
  if (config->interval >= TIMELAPSE_INTERVAL_COUNT || config->resolution >= TIMELAPSE_RESOLUTION_COUNT ||
      config->fragment >= TIMELAPSE_FRAGMENT_COUNT) return STORE_IO;
  const char *path = paths[generation & 1U];
  Store_Result_t result = Store_Remove(path);
  if (result != STORE_OK && result != STORE_NOT_FOUND) return result;
  Put32(data + 4, generation); data[8] = config->interval; data[9] = config->resolution;
  data[10] = config->fragment;
  Put32(data + 12, CRC(data, 12));
  result = Store_Open(&file, path, STORE_CREATE);
  if (result != STORE_OK) return result;
  result = Store_Write(&file, data, sizeof(data));
  if (result == STORE_OK) result = Store_Sync(&file);
  Store_Result_t closed = Store_Close(&file);
  if (result == STORE_OK) result = closed;
  if (result == STORE_OK) config->generation = generation;
  return result;
}
