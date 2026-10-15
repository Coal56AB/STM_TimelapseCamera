#include "esp_control.h"
#include "time_sync.h"
#include "usb_link.h"
#include <stdio.h>

uint8_t EspControl_Process(Clock_t *clock, uint8_t wifi_requested, uint32_t now)
{
  static uint32_t seen;
  static uint8_t wifi;
  char request[96], response[32];
  if (UsbLink_ReadLine(request))
  {
    unsigned state;
    char extra;
    if (sscanf(request, "STATE %u %c", &state, &extra) == 1 && state <= 1)
    {
      seen = now; wifi = (uint8_t)state;
      snprintf(response, sizeof(response), "WIFI %u\n", wifi_requested != 0);
    }
    else TimeSync_Command(clock, request, response, now);
    UsbLink_SendLine(response);
  }
  return wifi_requested && wifi && now - seen < 3000;
}
