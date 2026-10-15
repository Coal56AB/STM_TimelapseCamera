/** @brief Заготовка OV5640: определение ID; режим захвата пока не реализован. */
#include "sensor.h"
int OV5640_Probe(const Sensor_Bus_t *bus)
{
  uint8_t high, low;
  return bus->read(bus->context, 0x3c, 0x300a, 1, &high) &&
         bus->read(bus->context, 0x3c, 0x300b, 1, &low) && high == 0x56 && low == 0x40;
}
int OV5640_Init(const Sensor_Bus_t *bus, uint16_t width, uint16_t height)
{
  (void)bus; (void)width; (void)height;
  return 0;
}
