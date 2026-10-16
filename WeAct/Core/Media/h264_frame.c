#include "h264_frame.h"
#include "media_memory.h"
#include "avi.h"
#include "image_text.h"
#define H264E_SVC_API 0
#define H264E_MAX_THREADS 0
#define H264E_ENABLE_DENOISE 0
#define MINIH264_IMPLEMENTATION
#define MINIH264_NO_SIMD
#if defined(__ARMCC_VERSION)
#define NDEBUG
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "MiniH264/minih264e.h"
#pragma GCC diagnostic pop

static uint8_t luma[320 * 240] __attribute__((section(".bss.h264_luma"), aligned(32)));
static uint8_t chroma[320 * 240 / 2] __attribute__((section(".bss.h264_chroma"), aligned(32)));
static uint16_t width, height;

int H264_Begin(uint16_t w, uint16_t h, uint16_t fps)
{
  if (!w || !h || w > 320 || h > 240 || (w & 15) || (h & 1) || !fps || fps > 60) return 0;
  H264E_create_param_t config = {0};
  config.width = w; config.height = h;
  config.const_input_flag = (h & 15) != 0;
  config.gop = fps;
  int persistent, scratch;
  if (H264E_sizeof(&config, &persistent, &scratch) || persistent > (int)MEDIA_PERSIST_BYTES ||
      scratch > (int)MEDIA_SCRATCH_BYTES) return 0;
  if (H264E_init((H264E_persist_t *)Media_Memory, &config)) return 0;
  width = w; height = h; return 1;
}

int H264_Frame(const uint8_t *rgb, const char *timestamp, uint8_t **data, uint32_t *size)
{
  if (!rgb || !width) return 0;
  uint8_t rows[2][960];
  unsigned padded = (height + 15U) & ~15U, plane = width * padded / 4;
  for (unsigned y = 0; y < padded; y += 2)
  {
    for (unsigned r = 0; r < 2; ++r)
    {
      unsigned source = y + r < height ? y + r : height - 1U;
      AVI_ConvertRow(rows[r], rgb + source * width * 2U, width);
      ImageText_Row(rows[r], width, height, (uint16_t)source, timestamp);
      for (unsigned x = 0; x < width; ++x)
      {
        const uint8_t *p = rows[r] + x * 3;
        luma[(y + r) * width + x] = (uint8_t)(16 + ((66 * p[2] + 129 * p[1] + 25 * p[0] + 128) >> 8));
      }
    }
    for (unsigned x = 0; x < width; x += 2)
    {
      int b = 0, g = 0, r = 0;
      for (unsigned dy = 0; dy < 2; ++dy) for (unsigned dx = 0; dx < 2; ++dx)
      { const uint8_t *p = rows[dy] + (x + dx) * 3; b += p[0]; g += p[1]; r += p[2]; }
      unsigned at = y / 2 * (width / 2) + x / 2;
      chroma[at] = (uint8_t)(128 + ((-38 * r - 74 * g + 112 * b + 512) >> 10));
      chroma[plane + at] = (uint8_t)(128 + ((112 * r - 94 * g - 18 * b + 512) >> 10));
    }
  }
  H264E_io_yuv_t image = {{luma, chroma, chroma + plane}, {width, width / 2, width / 2}};
  H264E_run_param_t options = {0};
  options.encode_speed = H264E_SPEED_FASTEST; options.qp_min = 24; options.qp_max = 32;
  options.desired_frame_bytes = width * height / 24;
  int bytes;
  if (H264E_encode((H264E_persist_t *)Media_Memory, (H264E_scratch_t *)MEDIA_CAPTURE_BUFFER,
                  &options, &image, data, &bytes) || bytes <= 0) return 0;
  *size = (uint32_t)bytes; return 1;
}
