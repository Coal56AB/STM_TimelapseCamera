#include "clock.h"
#include <stdio.h>
int Clock_Set(Clock_t *c, uint32_t utc, int16_t offset, uint32_t now)
{
  if (utc < 946684800U || utc > 4102444799U || offset < -720 || offset > 840) return 0;
  *c = (Clock_t){utc, now, 0, offset, 1};
  return 1;
}
void Clock_Update(Clock_t *c, uint32_t now)
{
  uint64_t elapsed = (uint64_t)(uint32_t)(now - c->tick) + c->milliseconds;
  c->tick = now;
  if (!c->valid) return;
  uint64_t seconds = (uint64_t)c->seconds + elapsed / 1000;
  if (seconds > 4102444799U) { c->valid = 0; return; }
  c->seconds = (uint32_t)seconds; c->milliseconds = (uint16_t)(elapsed % 1000);
}
static int Leap(unsigned year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
void Clock_Format(const Clock_t *c, char text[20])
{
  text[0] = 0;
  if (!c->valid) return;
  uint32_t local = (uint32_t)((int64_t)c->seconds + c->offset_minutes * 60);
  uint32_t days = local / 86400, time = local % 86400;
  unsigned year = 1970, month = 0;
  while (days >= (uint32_t)(365 + Leap(year))) { days -= 365 + Leap(year); ++year; }
  static const uint8_t lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  for (;;)
  {
    unsigned length = lengths[month] + (month == 1 && Leap(year));
    if (days < length) break;
    days -= length; ++month;
  }
  snprintf(text, 20, "%02u.%02u.%04u %02u:%02u:%02u", (unsigned)days + 1, month + 1, year,
           (unsigned)(time / 3600), (unsigned)(time / 60 % 60), (unsigned)(time % 60));
}
