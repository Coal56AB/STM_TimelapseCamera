/** @brief OV7725 RGB565; последовательность регистров из примера WeAct. */
#include "sensor.h"
#include "ov7725_regs.h"
#define OV7725_ADDRESS 0x21U

static int Write(const Sensor_Bus_t *bus, uint8_t reg, uint8_t value)
{
  return bus->write(bus->context, OV7725_ADDRESS, reg, 0, value);
}

static int Read(const Sensor_Bus_t *bus, uint8_t reg, uint8_t *value)
{
  return bus->read(bus->context, OV7725_ADDRESS, reg, 0, value);
}

int OV7725_Probe(const Sensor_Bus_t *bus)
{
  uint8_t high, low;
  return bus->read(bus->context, OV7725_ADDRESS, 0x0a, 0, &high) &&
         bus->read(bus->context, OV7725_ADDRESS, 0x0b, 0, &low) &&
         high == 0x77 && low >= 0x21 && low <= 0x23;
}

int OV7725_SetSize(const Sensor_Bus_t *bus, uint16_t width, uint16_t height)
{
  if (!((width == 160 && height == 120) || (width == 320 && height == 240))) return 0;
  uint8_t com3, com7, href;
  if (!Read(bus, COM3, &com3) || !Read(bus, COM7, &com7) || !Read(bus, HREF, &href)) return 0;
  uint8_t flipped = (com3 & COM3_VFLIP) != 0;
  /* Порядок и компенсация вертикального flip — как в WeAct 08-DCMI2LCD. */
  if (!(Write(bus, HOUTSIZE, (uint8_t)(width >> 2)) &&
         Write(bus, VOUTSIZE, (uint8_t)(height >> 1)) && Write(bus, EXHCH, 0) &&
         Write(bus, HREF, (uint8_t)((href & 0xbf) | (flipped ? 0x40 : 0))) &&
         Write(bus, COM7, COM7_SET_RES(com7, COM7_RES_QVGA)) &&
         Write(bus, HSTART, 0x3f) && Write(bus, HSIZE, 0x50) &&
         Write(bus, VSTART, (uint8_t)(0x03 - flipped)) && Write(bus, VSIZE, 0x78) &&
         Write(bus, DSPAUTO, 0xff))) return 0;
  uint8_t actual;
  return Read(bus, COM7, &actual) && actual == (COM7_RES_QVGA | COM7_FMT_RGB565 | COM7_FMT_RGB) &&
         Read(bus, DSP_CTRL4, &actual) && actual == DSP_CTRL4_YUV_RGB &&
         Read(bus, HOUTSIZE, &actual) && actual == width / 4 &&
         Read(bus, VOUTSIZE, &actual) && actual == height / 2;
}

int OV7725_Init(const Sensor_Bus_t *bus, uint16_t width, uint16_t height)
{
  if (!OV7725_Probe(bus) || !Write(bus, COM7, COM7_RESET)) return 0;
  bus->delay(10);
  for (unsigned i = 0; ov7725_default_regs[i][0]; ++i)
  {
    if (!Write(bus, ov7725_default_regs[i][0], ov7725_default_regs[i][1])) return 0;
  }
  /* WeAct выдерживает паузу после таблицы, до смены формата и окна. */
  bus->delay(300);
  uint8_t com7, com3;
  if (!Read(bus, COM7, &com7) || !Write(bus, DSP_CTRL4, DSP_CTRL4_YUV_RGB) ||
      !Write(bus, COM7, COM7_SET_FMT(com7, COM7_FMT_RGB)) ||
      !OV7725_SetSize(bus, width, height)) return 0;
  /* Ориентация штатного разъёма; порядок байтов выхода сохраняется. */
  if (!Read(bus, COM3, &com3)) return 0;
  com3 &= (uint8_t)~(COM3_MIRROR | COM3_VFLIP);
  if (!Write(bus, COM3, com3) ||
      !OV7725_SetSize(bus, width, height)) return 0;
  if (!Read(bus, COM3, &com7) || com7 != com3) return 0;
  /* Новые формат и окно должны примениться до первого snapshot. */
  bus->delay(300);
  return 1;
}
