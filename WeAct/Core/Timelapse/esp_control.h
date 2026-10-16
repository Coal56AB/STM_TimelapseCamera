#ifndef ESP_CONTROL_H
#define ESP_CONTROL_H
#include "clock.h"
/* Возвращает подтверждённое ESP состояние точки доступа. */
uint8_t EspControl_Process(Clock_t *clock, uint8_t wifi_requested, uint8_t sd_ready, uint32_t now);
const char *EspControl_Ssid(void);
const char *EspControl_Password(void);
#endif
