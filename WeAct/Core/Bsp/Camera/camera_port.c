#include "camera.h"
#include "sensor.h"
#include "dcmi.h"
#include "i2c.h"

/* DMA1 не видит DTCM; секция размещается линкером в AXI SRAM. */
static uint8_t pixels[320 * 240 * 2] __attribute__((section(".bss.dma_buffer"), aligned(32)));
static volatile uint8_t completed, failed;
static uint8_t running, initialized;
static uint16_t frame_width, frame_height;
static uint32_t started_at;

static int Read(void *context, uint8_t address, uint16_t reg, uint8_t wide, uint8_t *value)
{
  I2C_HandleTypeDef *i2c = context;
  if (wide) return HAL_I2C_Mem_Read(i2c, (uint16_t)(address << 1), reg, I2C_MEMADD_SIZE_16BIT, value, 1, 50) == HAL_OK;
  uint8_t byte = (uint8_t)reg;
  /* SCCB: STOP между передачей адреса регистра и чтением. */
  return HAL_I2C_Master_Transmit(i2c, (uint16_t)(address << 1), &byte, 1, 50) == HAL_OK &&
         HAL_I2C_Master_Receive(i2c, (uint16_t)(address << 1), value, 1, 50) == HAL_OK;
}
static int Write(void *context, uint8_t address, uint16_t reg, uint8_t wide, uint8_t value)
{
  if (!wide)
  {
    uint8_t data[2] = {(uint8_t)reg, value};
    return HAL_I2C_Master_Transmit(context, (uint16_t)(address << 1), data, 2, 50) == HAL_OK;
  }
  return HAL_I2C_Mem_Write(context, (uint16_t)(address << 1), reg,
                           wide ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT, &value, 1, 50) == HAL_OK;
}
static const Sensor_Bus_t bus = { &hi2c1, Read, Write, HAL_Delay };

Camera_Result_t Camera_Init(uint16_t width, uint16_t height)
{
  Camera_Stop();
  initialized = 0;
  if (!OV7725_Probe(&bus)) return OV5640_Probe(&bus) ? CAMERA_UNSUPPORTED : CAMERA_NOT_FOUND;
  if (!OV7725_Init(&bus, width, height)) return CAMERA_ERROR;
  frame_width = width; frame_height = height; initialized = 1;
  return CAMERA_OK;
}
Camera_Result_t Camera_SetSize(uint16_t width, uint16_t height)
{
  Camera_Stop();
  if (!initialized || !OV7725_SetSize(&bus, width, height)) { initialized = 0; return CAMERA_ERROR; }
  frame_width = width; frame_height = height;
  HAL_Delay(100);
  return CAMERA_OK;
}
Camera_Result_t Camera_Start(uint32_t now)
{
  if (!initialized) return CAMERA_ERROR;
  if (running) return CAMERA_BUSY;
  uint32_t bytes = (uint32_t)frame_width * frame_height * 2;
  completed = failed = 0;
  /* HAL выключает эти IRQ после snapshot, но Start_DMA их не восстанавливает. */
  __HAL_DCMI_DISABLE_IT(&hdcmi, DCMI_IT_FRAME | DCMI_IT_LINE | DCMI_IT_VSYNC);
  __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI | DCMI_FLAG_ERRRI |
                               DCMI_FLAG_VSYNCRI | DCMI_FLAG_LINERI);
  hdcmi.ErrorCode = HAL_DCMI_ERROR_NONE;
  __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_ERR | DCMI_IT_OVR);
  /* Отдаём целые отдельные cache lines DMA, исключая грязные строки CPU. */
  SCB_CleanInvalidateDCache_by_Addr((uint32_t *)pixels, (int32_t)bytes);
  __DSB();
  if (HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)(uintptr_t)pixels, bytes / 4) != HAL_OK)
  {
    HAL_DCMI_Stop(&hdcmi);
    return CAMERA_DMA_ERROR;
  }
  running = 1; started_at = now;
  return CAMERA_OK;
}
Camera_Result_t Camera_Process(uint32_t now, Camera_Frame_t *frame)
{
  if (!running) return CAMERA_BUSY;
  if (failed)
  {
    uint32_t error = hdcmi.ErrorCode;
    Camera_Stop();
    if (error & HAL_DCMI_ERROR_OVR) return CAMERA_OVERRUN;
    if (error & HAL_DCMI_ERROR_DMA) return CAMERA_DMA_ERROR;
    return CAMERA_SYNC_ERROR;
  }
  /* FRAME callback HAL разрешается после DMA transfer complete. Дополнительно проверяем NDTR. */
  if (!completed || __HAL_DMA_GET_COUNTER(hdcmi.DMA_Handle) != 0)
  {
    if (now - started_at < 1000) return CAMERA_BUSY;
    Camera_Stop();
    return CAMERA_TIMEOUT;
  }
  Camera_Stop();
  SCB_InvalidateDCache_by_Addr((uint32_t *)pixels, (int32_t)((uint32_t)frame_width * frame_height * 2));
  __DSB();
  frame->rgb565_be = pixels; frame->width = frame_width; frame->height = frame_height;
  return CAMERA_OK;
}
void Camera_Stop(void)
{
  if (running) HAL_DCMI_Stop(&hdcmi);
  running = 0;
}
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *handle)
{
  if (handle == &hdcmi) completed = 1;
}
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *handle)
{
  if (handle == &hdcmi) failed = 1;
}
