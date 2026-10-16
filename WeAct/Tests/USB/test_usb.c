#include "usb_link.h"
#include "usbh_cdc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static CDC_HandleTypeDef cdc;
static USBH_ClassTypeDef cls = {&cdc};
USBH_HandleTypeDef hUsbHostHS = {&cls, {1}};
ApplicationTypeDef Appli_state;
static uint32_t tick, resets, restarts, received;
static uint8_t *rx;
uint32_t HAL_GetTick(void) { return tick; }
USBH_StatusTypeDef USBH_Process(USBH_HandleTypeDef *h) { (void)h; return USBH_OK; }
USBH_StatusTypeDef USBH_ReEnumerate(USBH_HandleTypeDef *h) { (void)h; ++resets; return USBH_OK; }
USBH_StatusTypeDef USBH_Stop(USBH_HandleTypeDef *h) { (void)h; return USBH_OK; }
USBH_StatusTypeDef USBH_DeInit(USBH_HandleTypeDef *h) { (void)h; return USBH_OK; }
void MX_USB_HOST_Init(void) { ++restarts; }
USBH_StatusTypeDef USBH_CDC_Receive(USBH_HandleTypeDef *h, uint8_t *data, uint32_t size)
{ (void)h; assert(size == 64); rx = data; return USBH_OK; }
USBH_StatusTypeDef USBH_CDC_Transmit(USBH_HandleTypeDef *h, uint8_t *data, uint32_t size)
{ (void)h; (void)data; cdc.TxDataLength = size; return USBH_OK; }
uint32_t USBH_CDC_GetLastReceivedDataSize(USBH_HandleTypeDef *h) { (void)h; return received; }
int main(void)
{
  char line[96]; assert(!UsbLink_ReadLine(line));
  Appli_state = APPLICATION_READY; assert(!UsbLink_ReadLine(line));
  assert(UsbLink_SendLine("WIFI 1\n"));
  tick = 1999; assert(!UsbLink_ReadLine(line) && !resets);
  --cdc.TxDataLength; tick = 2100; assert(!UsbLink_ReadLine(line) && !resets);
  tick = 4100; assert(!UsbLink_ReadLine(line) && resets == 1);
  assert(!UsbLink_ReadLine(line));
  tick += 10000; assert(!UsbLink_ReadLine(line) && restarts == 1);
  Appli_state = APPLICATION_READY; assert(!UsbLink_ReadLine(line));
  received = 8; memcpy(rx, "STATE 1\n", 8); USBH_CDC_ReceiveCallback(&hUsbHostHS);
  assert(UsbLink_ReadLine(line) && !strcmp(line, "STATE 1"));
  tick += 7999; assert(!UsbLink_ReadLine(line) && resets == 1);
  tick += 1; assert(!UsbLink_ReadLine(line) && resets == 2);
  Appli_state = APPLICATION_READY; tick = UINT32_MAX - 500; assert(!UsbLink_ReadLine(line));
  tick += 7999; assert(!UsbLink_ReadLine(line) && resets == 2);
  tick += 1; assert(!UsbLink_ReadLine(line) && resets == 3);
  puts("PASS: CDC stalled TX recovery, progress timeout, enumeration restart, RX silence and tick wrap");
}
