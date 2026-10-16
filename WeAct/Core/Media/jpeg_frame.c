#include "jpeg_frame.h"
#include "image_text.h"
#include "avi.h"
#include "JpegEnc/JPEGENC.h"
/* Portable integer JPEG encoder; see JpegEnc/LICENSE and development.md. */
#include "JpegEnc/jpegenc.inl"

static uint8_t output[65536] __attribute__((section(".bss.dma_buffer"), aligned(32)));
static JPEGE_IMAGE encoder;
static uint8_t rows[16 * 320 * 3];

int JPEG_Frame(const uint8_t *rgb, uint16_t width, uint16_t height,
               const char *timestamp, const uint8_t **data, uint32_t *size)
{
  if (!rgb || !width || width > 320 || !height || height > 240) return 0;
  memset(&encoder, 0, sizeof(encoder));
  encoder.pOutput = output; encoder.iBufferSize = sizeof(output);
  encoder.pHighWater = output + sizeof(output) - 4096;
  JPEGENCODE state;
  if (JPEGEncodeBegin(&encoder, &state, width, height, JPEGE_PIXEL_RGB888,
                      JPEGE_SUBSAMPLE_420, JPEGE_Q_HIGH) != JPEGE_SUCCESS) return 0;
  unsigned pitch = (width + 15U) & ~15U;
  for (unsigned y = 0; y < height; y += 16)
  {
    for (unsigned row = 0; row < 16; ++row)
    {
      unsigned source_y = y + row < height ? y + row : height - 1U;
      uint8_t *line = rows + row * pitch * 3;
      AVI_ConvertRow(line, rgb + source_y * width * 2U, width);
      ImageText_Row(line, width, height, (uint16_t)source_y, timestamp);
      /* This encoder's RGB888 input uses BGR byte order. */
      for (unsigned x = width; x < pitch; ++x) memcpy(line + x * 3, line + (width - 1) * 3, 3);
    }
    for (unsigned x = 0; x < width; x += 16)
      if (JPEGAddMCU(&encoder, &state, rows + x * 3, (int)pitch * 3) != JPEGE_SUCCESS) return 0;
  }
  int length = JPEGEncodeEnd(&encoder);
  if (length <= 0 || (unsigned)length > sizeof(output)) return 0;
  *data = output; *size = (uint32_t)length;
  return 1;
}
