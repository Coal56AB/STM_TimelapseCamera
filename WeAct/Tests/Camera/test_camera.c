#include "camera.h"
#include "dcmi.h"
#include "i2c.h"
#include "ov7725_regs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static DMA_HandleTypeDef dma;
DCMI_HandleTypeDef hdcmi = {&dma, 0, 0, 0};
I2C_HandleTypeDef hi2c1;
static uint8_t regs[256], selected;
static uint32_t tick, last_write;
static int broken_register, start_failed, invalidated, stopped;
void HAL_Delay(uint32_t ms) { tick += ms; }
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t address, uint8_t *data, uint16_t length, uint32_t timeout)
{
  (void)h; (void)timeout;
  if (address != 0x42) return HAL_ERROR;
  assert(length == 1 || length == 2); selected = data[0];
  if (length == 2)
  {
    if (selected == COM7 && data[1] == COM7_RESET)
    {
      memset(regs, 0, sizeof(regs)); regs[COM3] = 0x10;
      regs[0x0a] = 0x77; regs[0x0b] = 0x21;
    }
    else if (!broken_register || selected != COM7) regs[selected] = data[1];
    last_write = tick;
  }
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h, uint16_t address, uint8_t *data, uint16_t length, uint32_t timeout)
{
  (void)h; (void)timeout; assert(length == 1);
  if (address != 0x42) return HAL_ERROR;
  *data = regs[selected]; return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h, uint16_t a, uint16_t r, uint16_t w, uint8_t *v, uint16_t n, uint32_t t)
{ (void)h; (void)a; (void)r; (void)w; (void)v; (void)n; (void)t; return HAL_ERROR; }
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h, uint16_t a, uint16_t r, uint16_t w, uint8_t *v, uint16_t n, uint32_t t)
{ (void)h; (void)a; (void)r; (void)w; (void)v; (void)n; (void)t; return HAL_ERROR; }
void SCB_CleanInvalidateDCache_by_Addr(uint32_t *p, int32_t n)
{ assert(!((uintptr_t)p & 31) && n > 0 && !(n & 31)); }
void SCB_InvalidateDCache_by_Addr(uint32_t *p, int32_t n)
{ SCB_CleanInvalidateDCache_by_Addr(p, n); ++invalidated; }
HAL_StatusTypeDef HAL_DCMI_Start_DMA(DCMI_HandleTypeDef *h, uint32_t mode, uint32_t data, uint32_t count)
{
  (void)data; assert(mode == DCMI_MODE_SNAPSHOT && tick - last_write >= 100);
  assert(h->mask == (DCMI_IT_ERR | DCMI_IT_OVR) && !h->flags && !h->ErrorCode);
  dma.remaining = count; return start_failed ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_DCMI_Stop(DCMI_HandleTypeDef *h) { (void)h; ++stopped; return HAL_OK; }
int main(void)
{
  static uint8_t camera_pixels[320 * 240 * 2] __attribute__((aligned(32)));
  Camera_SetBuffer(camera_pixels);
  regs[0x0a] = 0x77; regs[0x0b] = 0x21;
  assert(Camera_Init(320, 240) == CAMERA_OK && tick - last_write >= 300);
  assert(regs[COM7] == 0x46 && regs[DSP_CTRL4] == 0 && regs[COM3] == 0x10);
  assert(regs[HOUTSIZE] == 80 && regs[VOUTSIZE] == 120 && regs[VSTART] == 3 && !(regs[HREF] & 0x40));
  Camera_Frame_t frame;
  for (unsigned i = 0; i < 3; ++i)
  {
    hdcmi.flags = 31; hdcmi.mask = 31;
    assert(Camera_Start(tick) == CAMERA_OK && dma.remaining == 38400);
    assert(Camera_Process(tick, &frame) == CAMERA_BUSY);
    dma.remaining = 0; hdcmi.mask = 0; HAL_DCMI_FrameEventCallback(&hdcmi);
    /* SD может задержать основной цикл; уже готовый кадр не становится timeout. */
    assert(Camera_Process(tick + 2000, &frame) == CAMERA_OK);
    assert(frame.width == 320 && frame.height == 240);
  }
  assert(invalidated == 3);
  assert(Camera_Start(UINT32_MAX - 10) == CAMERA_OK);
  assert(Camera_Process(5, &frame) == CAMERA_BUSY);
  assert(Camera_Process(1000, &frame) == CAMERA_TIMEOUT);
  assert(Camera_Start(tick) == CAMERA_OK);
  hdcmi.ErrorCode = HAL_DCMI_ERROR_OVR; HAL_DCMI_ErrorCallback(&hdcmi);
  assert(Camera_Process(tick, &frame) == CAMERA_OVERRUN);
  start_failed = 1; int before = stopped;
  assert(Camera_Start(tick) == CAMERA_DMA_ERROR && stopped == before + 1); start_failed = 0;
  assert(Camera_Init(160, 120) == CAMERA_OK && regs[HOUTSIZE] == 40 && regs[VOUTSIZE] == 60);
  broken_register = 1; assert(Camera_Init(320, 240) == CAMERA_ERROR);
  assert(Camera_Start(tick) == CAMERA_ERROR);
  puts("PASS: OV7725 startup delay, orientation, RGB565 readback; snapshot rearm, late completion, timeout, DMA/overrun");
  return 0;
}
