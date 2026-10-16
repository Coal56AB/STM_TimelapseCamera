#include "recording_mp4.h"
#include "h264_frame.h"
#include <string.h>

Store_Result_t Recording_MP4Finish(Recording_t *r)
{
  if (!r->file.handle) return STORE_OK;
  uint8_t header[MP4_HEADER_SIZE]; char tmp[40], final[40];
  r->mp4.frames = r->fragment_frames;
  Store_Result_t result = MP4_Header(header, &r->mp4) ? Store_Seek(&r->file, 0) : STORE_IO;
  if (result == STORE_OK) result = Store_Write(&r->file, header, sizeof(header));
  if (result == STORE_OK) result = Store_Sync(&r->file);
  Store_Result_t closed = Store_Close(&r->file); if (result == STORE_OK) result = closed;
  if (result != STORE_OK) return result;
  Recording_Path(tmp, r->id, r->fragment, "TMP"); Recording_Path(final, r->id, r->fragment, "MP4");
  result = Store_Rename(tmp, final);
  if (result == STORE_OK) { ++r->fragment; r->fragment_frames = r->data_bytes = 0; }
  return result;
}

Store_Result_t Recording_MP4Append(Recording_t *r, const uint8_t *rgb, const char *timestamp)
{
  if (!r->active || !rgb || r->fragment > RECORDING_MAX_ID || r->frames == UINT32_MAX) return STORE_IO;
  Store_Result_t result = STORE_IO;
  if (!r->fragment_frames)
  {
    memset(&r->mp4, 0, sizeof(r->mp4)); r->mp4.format = r->format;
    if (!H264_Begin(r->format.width, r->format.height, r->format.fps)) goto failed;
  }
  uint8_t *data, key, header[MP4_HEADER_SIZE]; uint32_t size;
  if (!H264_Frame(rgb, timestamp, &data, &size) || !MP4_Sample(&r->mp4, data, &size, &key)) goto failed;
  if (!r->file.handle)
  {
    char path[40]; Recording_Path(path, r->id, r->fragment, "TMP");
    if (!key || !MP4_Header(header, &r->mp4)) goto failed;
    result = Store_Open(&r->file, path, STORE_CREATE); if (result != STORE_OK) goto failed;
    result = Store_Write(&r->file, header, MP4_HEADER_SIZE); if (result != STORE_OK) goto failed;
  }
  MP4_Packet(header, r->fragment_frames, size, key);
  result = Store_Write(&r->file, header, MP4_PACKET_SIZE);
  if (result == STORE_OK) result = Store_Write(&r->file, data, size);
  if (result == STORE_OK && (r->fragment_frames + 1) % 25 == 0) result = Store_Sync(&r->file);
  if (result != STORE_OK) goto failed;
  ++r->frames; ++r->fragment_frames; r->data_bytes += MP4_PACKET_SIZE + size;
  if (r->fragment_frames == r->fragment_limit)
  { result = Recording_MP4Finish(r); if (result != STORE_OK) goto failed; }
  return STORE_OK;
failed:
  Store_Close(&r->file); r->active = 0; return result;
}
