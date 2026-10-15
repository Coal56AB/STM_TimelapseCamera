#include "esp_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *incoming;
static char sent[32];
int UsbLink_ReadLine(char line[96])
{
  if (!incoming) return 0;
  strcpy(line, incoming); incoming = NULL; return 1;
}
int UsbLink_SendLine(const char *line) { strcpy(sent, line); return 1; }
int main(void)
{
  Clock_t clock = {0};
  incoming = "STATE 0";
  assert(!EspControl_Process(&clock, 0, 0)); assert(!strcmp(sent, "WIFI 0\n"));
  incoming = "STATE 0";
  assert(!EspControl_Process(&clock, 1, 1000)); assert(!strcmp(sent, "WIFI 1\n"));
  incoming = "STATE 1";
  assert(EspControl_Process(&clock, 1, 2000));
  assert(EspControl_Process(&clock, 1, 4999));
  assert(!EspControl_Process(&clock, 1, 5000));
  incoming = "STATE 1";
  assert(!EspControl_Process(&clock, 0, 6000)); assert(!strcmp(sent, "WIFI 0\n"));
  incoming = "TIME 4 946684800 180";
  EspControl_Process(&clock, 1, 7000);
  assert(!strcmp(sent, "OK 4\n") && clock.valid && clock.seconds == 946684800);
  incoming = "STATE 1"; assert(EspControl_Process(&clock, 1, UINT32_MAX - 500));
  assert(EspControl_Process(&clock, 1, 1000));
  assert(!EspControl_Process(&clock, 1, 3000));
  puts("PASS: STM32 Wi-Fi command, confirmed state/expiry, USB clock dispatch");
}
