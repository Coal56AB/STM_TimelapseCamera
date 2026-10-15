/** @brief SDMMC1, polling I/O. Не требует DMA-доступности буферов FatFs. */
#include "ff.h"
#include "diskio.h"
#include "sdmmc.h"
static DSTATUS status = STA_NOINIT;

static int WaitReady(void)
{
  uint32_t start = HAL_GetTick();
  for (;;)
  {
    HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd1);
    if (state == HAL_SD_CARD_TRANSFER) return 1;
    if (state == HAL_SD_CARD_ERROR) { status = STA_NOINIT; return 0; }
    if (HAL_GetTick() - start >= 2000) { status = STA_NOINIT; return 0; }
  }
}
DSTATUS disk_initialize(BYTE drive)
{
  if (drive) return STA_NOINIT;
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd1.Init.ClockDiv = 2;
  status = STA_NOINIT;
  if (HAL_SD_Init(&hsd1) == HAL_OK && HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) == HAL_OK)
    status = WaitReady() ? 0 : STA_NOINIT;
  return status;
}
DSTATUS disk_status(BYTE drive) { return drive ? STA_NOINIT : status; }
DRESULT disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
  if (drive || status || !count) return RES_NOTRDY;
  if (HAL_SD_ReadBlocks(&hsd1, data, (uint32_t)sector, count, 2000) != HAL_OK || !WaitReady())
  { status = STA_NOINIT; return RES_ERROR; }
  return RES_OK;
}
DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
  if (drive || status || !count) return RES_NOTRDY;
  if (HAL_SD_WriteBlocks(&hsd1, (uint8_t *)data, (uint32_t)sector, count, 2000) != HAL_OK || !WaitReady())
  { status = STA_NOINIT; return RES_ERROR; }
  return RES_OK;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *data)
{
  if (!drive && command == CTRL_EJECT)
  {
    if (hsd1.Instance == SDMMC1) HAL_SD_DeInit(&hsd1);
    status = STA_NOINIT;
    return RES_OK;
  }
  if (drive || status) return RES_NOTRDY;
  switch (command)
  {
    case CTRL_SYNC: return WaitReady() ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT: *(LBA_t *)data = hsd1.SdCard.LogBlockNbr; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)data = 512; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)data = 1; return RES_OK;
    default: return RES_PARERR;
  }
}
