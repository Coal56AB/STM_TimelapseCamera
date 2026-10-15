#ifndef CLOCK_H
#define CLOCK_H
#include <stdint.h>
typedef struct
{
  uint32_t seconds, tick;
  uint16_t milliseconds;
  int16_t offset_minutes;
  uint8_t valid;
} Clock_t;
int Clock_Set(Clock_t *clock, uint32_t utc, int16_t offset_minutes, uint32_t now);
void Clock_Update(Clock_t *clock, uint32_t now);
/* DD.MM.YYYY HH:MM:SS, либо пустая строка до установки времени. */
void Clock_Format(const Clock_t *clock, char text[20]);
#endif
