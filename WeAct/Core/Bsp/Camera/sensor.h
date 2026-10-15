/** @brief SCCB-интерфейс датчика без зависимости от STM32 HAL. */
#ifndef SENSOR_H
#define SENSOR_H
#include <stdint.h>
typedef struct
{
  void *context;
  int (*read)(void *context, uint8_t address, uint16_t reg, uint8_t wide, uint8_t *value);
  int (*write)(void *context, uint8_t address, uint16_t reg, uint8_t wide, uint8_t value);
  void (*delay)(uint32_t ms);
} Sensor_Bus_t;
int OV7725_Probe(const Sensor_Bus_t *bus);
int OV7725_Init(const Sensor_Bus_t *bus, uint16_t width, uint16_t height);
int OV7725_SetSize(const Sensor_Bus_t *bus, uint16_t width, uint16_t height);
int OV5640_Probe(const Sensor_Bus_t *bus);
int OV5640_Init(const Sensor_Bus_t *bus, uint16_t width, uint16_t height);
#endif
