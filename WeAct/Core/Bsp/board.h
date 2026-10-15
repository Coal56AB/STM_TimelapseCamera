#ifndef BOARD_H
#define BOARD_H
#include "button.h"
#include <stdint.h>
void Board_Init(void);
void Board_Tick(void);
Button_Event_t Board_Button(void);
uint32_t Board_Time(void);
void Board_Led(uint8_t on);
#endif
