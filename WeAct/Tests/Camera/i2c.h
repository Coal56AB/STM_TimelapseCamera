#ifndef TEST_I2C_H
#define TEST_I2C_H
#include "dcmi.h"
typedef struct { unsigned unused; } I2C_HandleTypeDef;
extern I2C_HandleTypeDef hi2c1;
#define I2C_MEMADD_SIZE_16BIT 2
#define I2C_MEMADD_SIZE_8BIT 1
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
void HAL_Delay(uint32_t ms);
#endif
