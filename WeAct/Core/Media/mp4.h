#ifndef MP4_H
#define MP4_H
#include "avi.h"
#define MP4_HEADER_SIZE 1024U
#define MP4_PACKET_SIZE 108U
typedef struct {
  AVI_Format_t format;
  uint32_t frames;
  uint8_t sps[64], pps[64], sps_size, pps_size;
} MP4_Info_t;
/* Convert encoder Annex B to AVC samples in place, retaining SPS/PPS in info. */
int MP4_Sample(MP4_Info_t *info, uint8_t *data, uint32_t *size, uint8_t *key);
int MP4_Header(uint8_t header[MP4_HEADER_SIZE], const MP4_Info_t *info);
int MP4_Parse(const uint8_t header[MP4_HEADER_SIZE], MP4_Info_t *info);
void MP4_Packet(uint8_t header[MP4_PACKET_SIZE], uint32_t frame, uint32_t size, uint8_t key);
int MP4_ParsePacket(const uint8_t header[MP4_PACKET_SIZE], uint32_t frame, uint32_t *size, uint8_t *key);
#endif
