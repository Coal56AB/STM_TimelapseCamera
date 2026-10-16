#include "store.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>

static FATFS volume;
static struct { FIL file; uint8_t used; } files[3];
static uint32_t generation;
static int Valid(const Store_File_t *file)
{
  if (!file->handle || file->generation != generation) return 0;
  for (unsigned i = 0; i < 3; ++i) if (file->handle == &files[i].file) return files[i].used;
  return 0;
}

static Store_Result_t Result(FRESULT result)
{
  switch (result)
  {
    case FR_OK: return STORE_OK;
    case FR_NO_FILE: case FR_NO_PATH: return STORE_NOT_FOUND;
    case FR_EXIST: return STORE_EXISTS;
    default: return STORE_IO;
  }
}
void Store_Reset(void)
{
  f_mount(NULL, "0:", 0);
  memset(files, 0, sizeof(files));
  ++generation;
  disk_ioctl(0, CTRL_EJECT, NULL);
}
Store_Result_t Store_Init(void)
{
  Store_Reset();
  return Result(f_mount(&volume, "0:", 1));
}
Store_Result_t Store_Check(void)
{
  return disk_status(0) == 0 && disk_ioctl(0, CTRL_SYNC, NULL) == RES_OK ? STORE_OK : STORE_IO;
}
Store_Result_t Store_Open(Store_File_t *file, const char *path, Store_Mode_t mode)
{
  if (file->handle) return STORE_IO;
  for (unsigned i = 0; i < 3; ++i)
  {
    if (files[i].used) continue;
    FRESULT result = f_open(&files[i].file, path, mode == STORE_READ ? FA_READ : FA_WRITE | FA_CREATE_NEW);
    if (result != FR_OK) return Result(result);
    files[i].used = 1;
    file->handle = &files[i].file;
    file->generation = generation;
    return STORE_OK;
  }
  return STORE_IO;
}
Store_Result_t Store_Read(Store_File_t *file, void *data, uint32_t length)
{
  UINT read;
  if (!Valid(file)) return STORE_IO;
  FRESULT result = f_read(file->handle, data, length, &read);
  return result == FR_OK && read == length ? STORE_OK : STORE_IO;
}
Store_Result_t Store_Write(Store_File_t *file, const void *data, uint32_t length)
{
  UINT written;
  if (!Valid(file)) return STORE_IO;
  FRESULT result = f_write(file->handle, data, length, &written);
  if (result != FR_OK) return Result(result);
  return written == length ? STORE_OK : STORE_FULL;
}
Store_Result_t Store_Seek(Store_File_t *file, uint32_t offset)
{
  if (!Valid(file)) return STORE_IO;
  if (f_tell((FIL *)file->handle) == offset) return STORE_OK;
  FRESULT result = f_lseek(file->handle, offset);
  return result == FR_OK && f_tell((FIL *)file->handle) == offset ? STORE_OK : STORE_IO;
}
Store_Result_t Store_Sync(Store_File_t *file) { return Valid(file) ? Result(f_sync(file->handle)) : STORE_IO; }
Store_Result_t Store_Close(Store_File_t *file)
{
  if (!file->handle) return STORE_OK;
  if (!Valid(file)) { file->handle = NULL; return STORE_IO; }
  FRESULT result = f_close(file->handle);
  for (unsigned i = 0; i < 3; ++i) if (file->handle == &files[i].file) files[i].used = 0;
  file->handle = NULL;
  return Result(result);
}
Store_Result_t Store_Stat(const char *path, uint32_t *size)
{
  uint64_t value;
  Store_Result_t result = Store_Stat64(path, &value);
  if (result == STORE_OK && value > UINT32_MAX) return STORE_FULL;
  if (result == STORE_OK) *size = (uint32_t)value;
  return result;
}
Store_Result_t Store_Stat64(const char *path, uint64_t *size)
{
  FILINFO info;
  FRESULT result = f_stat(path, &info);
  if (result == FR_OK) *size = info.fsize;
  return Result(result);
}
Store_Result_t Store_Mkdir(const char *path) { return Result(f_mkdir(path)); }
Store_Result_t Store_FileLimit(uint64_t *bytes)
{
  if (!volume.fs_type) return STORE_IO;
  *bytes = volume.fs_type == FS_EXFAT ? UINT64_MAX : UINT32_MAX;
  return STORE_OK;
}
Store_Result_t Store_Rename(const char *from, const char *to) { return Result(f_rename(from, to)); }
Store_Result_t Store_Remove(const char *path) { return Result(f_unlink(path)); }
Store_Result_t Store_Scan(const char *path, Store_Visitor_t visitor, void *context)
{
  DIR directory;
  FILINFO info;
  FRESULT result = f_opendir(&directory, path);
  if (result != FR_OK) return Result(result);
  for (;;)
  {
    result = f_readdir(&directory, &info);
    if (result != FR_OK || !info.fname[0]) break;
    if (!visitor(context, info.fname, (info.fattrib & AM_DIR) != 0)) break;
  }
  FRESULT closed = f_closedir(&directory);
  return Result(result == FR_OK ? closed : result);
}
Store_Result_t Store_Free(uint64_t *bytes)
{
  DWORD clusters;
  FATFS *fs;
  FRESULT result = f_getfree("0:", &clusters, &fs);
  if (result == FR_OK) *bytes = (uint64_t)clusters * fs->csize * 512U;
  return Result(result);
}
