/** @brief API кадров камеры. Буфер принадлежит драйверу до следующего Start. */
#ifndef CAMERA_H
#define CAMERA_H
#include <stdint.h>
typedef enum
{
  CAMERA_OK, CAMERA_BUSY, CAMERA_ERROR, CAMERA_NOT_FOUND, CAMERA_UNSUPPORTED,
  CAMERA_TIMEOUT, CAMERA_OVERRUN, CAMERA_DMA_ERROR, CAMERA_SYNC_ERROR
} Camera_Result_t;
typedef struct { const uint8_t *rgb565_be; uint16_t width, height; } Camera_Frame_t;
Camera_Result_t Camera_Init(uint16_t width, uint16_t height);
Camera_Result_t Camera_SetSize(uint16_t width, uint16_t height);
Camera_Result_t Camera_Start(uint32_t now);
Camera_Result_t Camera_Process(uint32_t now, Camera_Frame_t *frame);
void Camera_Stop(void);
#endif
