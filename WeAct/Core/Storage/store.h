/** @file store.h
 *  @brief Файловый API. Реализация FatFs находится в Core/Bsp/Storage.
 *  @details Один владелец файловой системы: основной цикл. Пути — 8.3.
 */
#ifndef STORE_H
#define STORE_H
#include <stddef.h>
#include <stdint.h>

typedef struct { void *handle; uint32_t generation; } Store_File_t;
typedef enum { STORE_OK, STORE_NOT_FOUND, STORE_EXISTS, STORE_IO, STORE_FULL } Store_Result_t;
typedef enum { STORE_READ, STORE_CREATE } Store_Mode_t;
typedef int (*Store_Visitor_t)(void *context, const char *name, uint8_t directory);

Store_Result_t Store_Init(void);
Store_Result_t Store_Check(void);
/* Отсоединение без записи грязных буферов на извлечённую/заменённую карту. */
void Store_Reset(void);
Store_Result_t Store_Open(Store_File_t *file, const char *path, Store_Mode_t mode);
Store_Result_t Store_Read(Store_File_t *file, void *data, uint32_t length);
Store_Result_t Store_Write(Store_File_t *file, const void *data, uint32_t length);
Store_Result_t Store_Seek(Store_File_t *file, uint32_t offset);
Store_Result_t Store_Sync(Store_File_t *file);
Store_Result_t Store_Close(Store_File_t *file);
Store_Result_t Store_Stat64(const char *path, uint64_t *size);
Store_Result_t Store_Stat(const char *path, uint32_t *size);
Store_Result_t Store_Mkdir(const char *path);
Store_Result_t Store_Rename(const char *from, const char *to);
Store_Result_t Store_Remove(const char *path);
Store_Result_t Store_Scan(const char *path, Store_Visitor_t visitor, void *context);
Store_Result_t Store_Free(uint64_t *bytes);
Store_Result_t Store_FileLimit(uint64_t *bytes);
#endif
