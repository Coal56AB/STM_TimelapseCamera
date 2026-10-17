#include "usb_link.h"
#include "usb_host.h"
#include "usbh_cdc.h"
#include <string.h>
extern USBH_HandleTypeDef hUsbHostHS;
extern ApplicationTypeDef Appli_state;
static uint8_t rx[64];
static uint8_t tx[16480];
static char assembled[96];
static uint16_t received, cursor, length;
static uint8_t rx_pending, tx_pending, discard;
static uint8_t ready;
static uint32_t last_rx, tx_progress, tx_remaining;
static ApplicationTypeDef observed;
static uint32_t phase_since;
static void ResetLink(void)
{
  received = cursor = length = 0;
  rx_pending = tx_pending = discard = ready = 0;
}
int UsbLink_ReadLine(char line[96])
{
  uint32_t now = HAL_GetTick();
  if (observed != Appli_state) { observed = Appli_state; phase_since = now; }
  if (Appli_state != APPLICATION_READY)
  {
    ResetLink();
    if (hUsbHostHS.device.is_connected && now - phase_since >= 10000)
    {
      USBH_Stop(&hUsbHostHS); USBH_DeInit(&hUsbHostHS);
      Appli_state = APPLICATION_IDLE; phase_since = now;
      MX_USB_HOST_Init();
    }
    return 0;
  }
  if (!ready) { ready = 1; last_rx = now; }
  CDC_HandleTypeDef *cdc = hUsbHostHS.pActiveClass->pData;
  if (tx_pending && cdc->TxDataLength != tx_remaining)
  { tx_remaining = cdc->TxDataLength; tx_progress = now; }
  /* Recover stalled/error CDC states through normal USB enumeration. */
  if ((tx_pending && now - tx_progress >= 2000) || (!tx_pending && now - last_rx >= 8000))
  {
    ResetLink();
    Appli_state = APPLICATION_START;
    USBH_ReEnumerate(&hUsbHostHS);
    return 0;
  }
  /* Let IRQs finish packets while servicing CDC continuously for at most 1 ms.
   * A fixed small iteration count can expire before even one packet reaches the bus. */
  uint32_t started = HAL_GetTick();
  for (unsigned i = 0; tx_pending && i < 32768 && HAL_GetTick() - started < 1; ++i)
    USBH_Process(&hUsbHostHS);
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
  if (tx_pending) { tx_progress = HAL_GetTick(); tx_remaining = (uint32_t)n + length; }
  return tx_pending;
}
void USBH_CDC_ReceiveCallback(USBH_HandleTypeDef *host)
{
  if (host != &hUsbHostHS) return;
  last_rx = HAL_GetTick();
  received = USBH_CDC_GetLastReceivedDataSize(host);
  if (received > sizeof(rx)) received = 0;
  cursor = 0; rx_pending = 0;
}
void USBH_CDC_TransmitCallback(USBH_HandleTypeDef *host)
{
  if (host == &hUsbHostHS) tx_pending = 0;
}
