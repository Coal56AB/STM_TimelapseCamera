#include "esp_control.h"
#include "time_sync.h"
#include "usb_link.h"
#include <stdio.h>
#include <string.h>
#include "download.h"

uint8_t EspControl_Process(Clock_t *clock, uint8_t wifi_requested, uint8_t sd_ready, uint32_t now)
{
  static uint32_t seen;
  static uint8_t wifi;
  static Download_t download;
  static uint8_t payload[DOWNLOAD_BLOCK_SIZE] __attribute__((section(".bss.dma_buffer"), aligned(32)));
  char request[96], response[96];
  Download_Process(&download, wifi_requested && sd_ready, now);
  if (UsbLink_ReadLine(request))
  {
    unsigned state;
    char extra;
    if (sscanf(request, "STATE %u %c", &state, &extra) == 1 && state <= 1)
    {
      seen = now; wifi = (uint8_t)state;
      snprintf(response, sizeof(response), "WIFI %u\n", wifi_requested != 0);
    }
    else if (!strncmp(request, "FILES ", 6))
    {
      uint32_t length;
      Download_Command(&download, request, now, wifi_requested && sd_ready, response, payload, &length);
      UsbLink_SendPacket(response, payload, length);
      return wifi_requested && wifi && now - seen < 3000;
    }
    else TimeSync_Command(clock, request, response, now);
    UsbLink_SendLine(response);
  }
  return wifi_requested && wifi && now - seen < 3000;
}
