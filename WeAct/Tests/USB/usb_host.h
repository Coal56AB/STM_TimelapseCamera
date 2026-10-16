#ifndef TEST_USB_HOST_H
#define TEST_USB_HOST_H
#include <stdint.h>
typedef enum { APPLICATION_IDLE, APPLICATION_START, APPLICATION_READY, APPLICATION_DISCONNECT } ApplicationTypeDef;
typedef struct { void *pData; } USBH_ClassTypeDef;
typedef struct { USBH_ClassTypeDef *pActiveClass; struct { uint8_t is_connected; } device; } USBH_HandleTypeDef;
typedef enum { USBH_OK, USBH_FAIL } USBH_StatusTypeDef;
uint32_t HAL_GetTick(void);
USBH_StatusTypeDef USBH_Process(USBH_HandleTypeDef *host);
USBH_StatusTypeDef USBH_ReEnumerate(USBH_HandleTypeDef *host);
USBH_StatusTypeDef USBH_Stop(USBH_HandleTypeDef *host);
USBH_StatusTypeDef USBH_DeInit(USBH_HandleTypeDef *host);
void MX_USB_HOST_Init(void);
#endif
