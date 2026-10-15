#ifndef TEST_TIM_H
#define TEST_TIM_H
#include "spi.h"
typedef struct { uint32_t compare; } TIM_HandleTypeDef;
extern TIM_HandleTypeDef htim1;
#define TIM_CHANNEL_2 2
#define __HAL_TIM_SET_COMPARE(timer, channel, value) ((timer)->compare = (value))
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *timer, uint32_t channel);
#endif
