#include "clock.h"
#include "image_text.h"
#include "time_sync.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
  Clock_t clock = {0}; char text[20], reply[32];
  Clock_Format(&clock, text); assert(!text[0]);
  assert(Clock_Set(&clock, 946684800, 180, UINT32_MAX - 999));
  Clock_Format(&clock, text); assert(!strcmp(text, "01.01.2000 03:00:00"));
  Clock_Update(&clock, 1000); Clock_Format(&clock, text);
  assert(!strcmp(text, "01.01.2000 03:00:02"));
  assert(Clock_Set(&clock, 951782400, 0, 0));
  Clock_Format(&clock, text); assert(!strcmp(text, "29.02.2000 00:00:00"));
  assert(Clock_Set(&clock, 951782400, -60, 0));
  Clock_Format(&clock, text); assert(!strcmp(text, "28.02.2000 23:00:00"));
  assert(!Clock_Set(&clock, 946684799, 0, 0));
  assert(!Clock_Set(&clock, 951782400, 841, 0));
  TimeSync_Command(&clock, "TIME 41 946684800 180", reply, 50);
  assert(!strcmp(reply, "OK 41\n"));
  Clock_Format(&clock, text); assert(!strcmp(text, "01.01.2000 03:00:00"));
  TimeSync_Command(&clock, "TIME 42 946684800 180 bad", reply, 50);
  assert(!strcmp(reply, "ERR 42\n"));
  TimeSync_Command(&clock, "TIME 43 946684800 40000", reply, 50);
  assert(!strcmp(reply, "ERR 43\n"));
  unsigned white = 0, black = 0;
  for (uint16_t y = 0; y < 120; ++y)
  {
    uint8_t row[160 * 3]; memset(row, 127, sizeof(row));
    ImageText_Row(row, 160, 120, y, text);
    for (unsigned x = 0; x < 160; ++x)
    {
      assert(row[x * 3] == row[x * 3 + 1] && row[x * 3] == row[x * 3 + 2]);
      if (y < 105 || y >= 118 || x < 2 || x >= 117) assert(row[x * 3] == 127);
      white += row[x * 3] == 255; black += row[x * 3] == 0;
    }
  }
  assert(white > 100 && black > 100);
  puts("PASS: calendar, leap day, timezone, tick wrap, USB time command, bottom-left timestamp");
  return 0;
}
