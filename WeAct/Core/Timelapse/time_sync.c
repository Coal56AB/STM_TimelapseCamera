#include "time_sync.h"
#include <stdio.h>
void TimeSync_Command(Clock_t *clock, const char *request, char response[32], uint32_t now)
{
  unsigned long id = 0, utc = 0;
  long offset = 0;
  char extra;
  int ok = sscanf(request, "TIME %lu %lu %ld %c", &id, &utc, &offset, &extra) == 3 &&
           id <= UINT32_MAX && utc <= UINT32_MAX && offset >= -720 && offset <= 840 &&
           Clock_Set(clock, (uint32_t)utc, (int16_t)offset, now);
  snprintf(response, 32, "%s %lu\n", ok ? "OK" : "ERR", id);
}
