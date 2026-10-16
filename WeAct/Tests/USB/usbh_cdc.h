#ifndef TEST_USB_CDC_H
#define TEST_USB_CDC_H
#include "usb_host.h"
typedef struct { uint32_t TxDataLength; } CDC_HandleTypeDef;
USBH_StatusTypeDef USBH_CDC_Receive(USBH_HandleTypeDef *host, uint8_t *data, uint32_t size);
USBH_StatusTypeDef USBH_CDC_Transmit(USBH_HandleTypeDef *host, uint8_t *data, uint32_t size);
uint32_t USBH_CDC_GetLastReceivedDataSize(USBH_HandleTypeDef *host);
void USBH_CDC_ReceiveCallback(USBH_HandleTypeDef *host);
void USBH_CDC_TransmitCallback(USBH_HandleTypeDef *host);
#endif
