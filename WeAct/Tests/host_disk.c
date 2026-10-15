/* Разреженный образ SD; FAT16/FAT32/exFAT и отказ выбранной операции записи. */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
static FILE *disk;
static LBA_t sectors;
BYTE Host_FormatType = FM_FAT;
int Host_FailWrite = -1;
int Host_Present = 1;
static int initialized;
void Host_Format(void)
{
  if (disk) fclose(disk);
  disk = tmpfile();
  sectors = Host_FormatType == FM_EXFAT ? 125000000U : 131072U;
  assert(disk && ftruncate(fileno(disk), (off_t)sectors * 512) == 0);
  Host_FailWrite = -1;
  Host_Present = 1; initialized = 0;
  BYTE work[4096];
  MKFS_PARM options = {Host_FormatType, 0, 0, 0, Host_FormatType == FM_EXFAT ? 128 * 1024 : 0};
  assert(f_mkfs("0:", &options, work, sizeof(work)) == FR_OK);
}
DSTATUS disk_initialize(BYTE drive) { initialized = !drive && Host_Present; return initialized ? 0 : STA_NOINIT; }
DSTATUS disk_status(BYTE drive) { return drive || !Host_Present || !initialized ? STA_NOINIT : 0; }
DRESULT disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
  if (!Host_Present || !initialized) return RES_NOTRDY;
  if (drive || sector >= sectors || count > sectors - sector) return RES_PARERR;
  if (fseeko(disk, (off_t)sector * 512, SEEK_SET) != 0) return RES_ERROR;
  return fread(data, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
  if (!Host_Present || !initialized) return RES_NOTRDY;
  if (drive || sector >= sectors || count > sectors - sector) return RES_PARERR;
  if (Host_FailWrite == 0) return RES_ERROR;
  if (Host_FailWrite > 0) --Host_FailWrite;
  if (fseeko(disk, (off_t)sector * 512, SEEK_SET) != 0) return RES_ERROR;
  return fwrite(data, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *data)
{
  if (drive) return RES_PARERR;
  if (command == CTRL_EJECT) { initialized = 0; return RES_OK; }
  if (!Host_Present || !initialized) return RES_NOTRDY;
  switch (command)
  {
    case CTRL_SYNC: return fflush(disk) == 0 ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT: *(LBA_t *)data = sectors; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)data = 512; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)data = 1; return RES_OK;
    default: return RES_PARERR;
  }
}
