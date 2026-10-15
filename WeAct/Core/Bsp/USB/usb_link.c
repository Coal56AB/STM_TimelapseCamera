#include "usb_link.h"
#include "usb_host.h"
#include "usbh_cdc.h"
#include <string.h>
extern USBH_HandleTypeDef hUsbHostHS;
extern ApplicationTypeDef Appli_state;
static uint8_t rx[64], tx[4192];
static char assembled[96];
static uint16_t received, cursor, length;
static uint8_t rx_pending, tx_pending, discard;
int UsbLink_ReadLine(char line[96])
{
  if (Appli_state != APPLICATION_READY)
  {
    received = cursor = length = 0;
    rx_pending = tx_pending = discard = 0;
    return 0;
  }
  if (tx_pending) return 0;
  while (cursor < received)
  {
    char byte = (char)rx[cursor++];
    if (byte == '\n')
    {
      if (!discard && length)
      {
        assembled[length] = 0;
        memcpy(line, assembled, length + 1);
        length = 0;
        return 1;
      }
      length = 0; discard = 0;
    }
    else if (byte != '\r' && !discard)
    {
      if (length < sizeof(assembled) - 1) assembled[length++] = byte;
      else { discard = 1; length = 0; }
    }
  }
  if (!rx_pending)
  {
    received = cursor = 0;
    rx_pending = USBH_CDC_Receive(&hUsbHostHS, rx, sizeof(rx)) == USBH_OK;
  }
  return 0;
}
int UsbLink_SendLine(const char *line)
{
  return UsbLink_SendPacket(line, NULL, 0);
}
int UsbLink_SendPacket(const char *line, const uint8_t *data, uint32_t length)
{
  size_t n = strlen(line);
  if (Appli_state != APPLICATION_READY || tx_pending || n + length > sizeof(tx)) return 0;
  memcpy(tx, line, n);
  if (length) memcpy(tx + n, data, length);
  tx_pending = USBH_CDC_Transmit(&hUsbHostHS, tx, (uint32_t)n + length) == USBH_OK;
  return tx_pending;
}
void USBH_CDC_ReceiveCallback(USBH_HandleTypeDef *host)
{
  if (host != &hUsbHostHS) return;
  received = USBH_CDC_GetLastReceivedDataSize(host);
  if (received > sizeof(rx)) received = 0;
  cursor = 0; rx_pending = 0;
}
void USBH_CDC_TransmitCallback(USBH_HandleTypeDef *host)
{
  if (host == &hUsbHostHS) tx_pending = 0;
}
