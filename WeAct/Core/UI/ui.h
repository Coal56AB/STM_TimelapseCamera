#ifndef UI_H
#define UI_H
#include "button.h"
#include <stdint.h>
typedef enum { UI_PREVIEW, UI_MENU, UI_CONFIRM, UI_MERGING, UI_WIFI_SCREEN } UI_Screen_t;
typedef enum { UI_NO_ACTION, UI_INTERVAL, UI_RESOLUTION, UI_FRAGMENT, UI_WIFI, UI_MERGE, UI_CANCEL, UI_RECORD, UI_WIFI_EXIT } UI_Action_t;
typedef struct { UI_Screen_t screen; uint8_t item, dirty; } UI_t;
typedef struct
{
  const char *interval;
  uint32_t session, frames, percent;
  uint16_t width, height;
  uint8_t recording;
  const char *message;
  uint16_t fragment_seconds;
  uint32_t duration_seconds;
  uint8_t wifi_ready;
} UI_View_t;
void UI_Init(UI_t *ui);
UI_Action_t UI_Process(UI_t *ui, Button_Event_t event);
void UI_Draw(UI_t *ui, const UI_View_t *view);
#endif
