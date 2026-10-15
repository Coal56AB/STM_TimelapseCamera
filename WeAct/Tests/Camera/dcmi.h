#ifndef TEST_DCMI_H
#define TEST_DCMI_H
#include <stdint.h>
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
typedef struct { uint32_t remaining; } DMA_HandleTypeDef;
typedef struct { DMA_HandleTypeDef *DMA_Handle; uint32_t ErrorCode, mask, flags; } DCMI_HandleTypeDef;
extern DCMI_HandleTypeDef hdcmi;
#define DCMI_MODE_SNAPSHOT 1
#define DCMI_IT_FRAME 1
#define DCMI_IT_LINE 2
#define DCMI_IT_VSYNC 4
#define DCMI_IT_ERR 8
#define DCMI_IT_OVR 16
#define DCMI_FLAG_FRAMERI DCMI_IT_FRAME
#define DCMI_FLAG_LINERI DCMI_IT_LINE
#define DCMI_FLAG_VSYNCRI DCMI_IT_VSYNC
#define DCMI_FLAG_ERRRI DCMI_IT_ERR
#define DCMI_FLAG_OVRRI DCMI_IT_OVR
#define HAL_DCMI_ERROR_NONE 0
#define HAL_DCMI_ERROR_OVR 1
#define HAL_DCMI_ERROR_DMA 2
#define __HAL_DCMI_DISABLE_IT(h, bits) ((h)->mask &= ~(bits))
#define __HAL_DCMI_ENABLE_IT(h, bits) ((h)->mask |= (bits))
#define __HAL_DCMI_CLEAR_FLAG(h, bits) ((h)->flags &= ~(bits))
#define __HAL_DMA_GET_COUNTER(h) ((h)->remaining)
void SCB_CleanInvalidateDCache_by_Addr(uint32_t *data, int32_t length);
void SCB_InvalidateDCache_by_Addr(uint32_t *data, int32_t length);
#define __DSB() ((void)0)
HAL_StatusTypeDef HAL_DCMI_Start_DMA(DCMI_HandleTypeDef *handle, uint32_t mode, uint32_t data, uint32_t count);
HAL_StatusTypeDef HAL_DCMI_Stop(DCMI_HandleTypeDef *handle);
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *handle);
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *handle);
#endif
