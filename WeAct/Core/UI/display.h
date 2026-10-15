#ifndef DISPLAY_H
#define DISPLAY_H
#include "camera.h"
#include <stdint.h>
int Display_Init(void);
void Display_Clear(void);
void Display_Line(uint8_t row, const char *text, uint8_t selected);
void Display_OverlayLine(uint8_t row, const char *text, uint8_t highlighted);
void Display_Preview(const Camera_Frame_t *frame);
#endif
