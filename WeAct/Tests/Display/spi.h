#ifndef TEST_SPI_H
#define TEST_SPI_H
#include <stdint.h>
typedef struct { int unused; } SPI_HandleTypeDef;
typedef int HAL_StatusTypeDef;
extern SPI_HandleTypeDef hspi4;
#define HAL_OK 0
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define LCD_CS_GPIO_Port 0
#define LCD_CS_Pin 0
#define LCD_WR_RS_GPIO_Port 0
#define LCD_WR_RS_Pin 1
void HAL_GPIO_WritePin(int port, int pin, int value);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, uint8_t *data, uint16_t size, uint32_t timeout);
uint32_t HAL_GetTick(void);
#endif
