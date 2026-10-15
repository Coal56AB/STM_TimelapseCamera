#include "esp_control.h"
#include "download.h"
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
int UsbLink_SendPacket(const char *header, const uint8_t *data, uint32_t length)
{ (void)header; (void)data; (void)length; return 1; }
void Download_Process(Download_t *d, uint8_t enabled, uint32_t now) { (void)d; (void)enabled; (void)now; }
void Download_Command(Download_t *d, const char *r, uint32_t now, uint8_t enabled, char h[96], uint8_t data[4096], uint32_t *n)
{ (void)d; (void)r; (void)now; (void)enabled; (void)data; h[0] = 0; *n = 0; }
int main(void)
{
  Clock_t clock = {0};
  incoming = "STATE 0";
  assert(!EspControl_Process(&clock, 0, 1, 0)); assert(!strcmp(sent, "WIFI 0\n"));
  incoming = "STATE 0";
  assert(!EspControl_Process(&clock, 1, 1, 1000)); assert(!strcmp(sent, "WIFI 1\n"));
  incoming = "STATE 1";
  assert(EspControl_Process(&clock, 1, 1, 2000));
  assert(EspControl_Process(&clock, 1, 1, 4999));
  assert(!EspControl_Process(&clock, 1, 1, 5000));
  incoming = "STATE 1";
  assert(!EspControl_Process(&clock, 0, 1, 6000)); assert(!strcmp(sent, "WIFI 0\n"));
  incoming = "TIME 4 946684800 180";
  EspControl_Process(&clock, 1, 1, 7000);
  assert(!strcmp(sent, "OK 4\n") && clock.valid && clock.seconds == 946684800);
  incoming = "STATE 1"; assert(EspControl_Process(&clock, 1, 1, UINT32_MAX - 500));
  assert(EspControl_Process(&clock, 1, 1, 1000));
  assert(!EspControl_Process(&clock, 1, 1, 3000));
  puts("PASS: STM32 Wi-Fi command, confirmed state/expiry, USB clock dispatch");
}
