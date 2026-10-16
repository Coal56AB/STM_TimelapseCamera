#include "ui.h"
#include "display.h"
#include <stdio.h>
#include <string.h>

void UI_Init(UI_t *ui) { *ui = (UI_t){UI_PREVIEW, 0, 1}; }
UI_Action_t UI_Process(UI_t *ui, Button_Event_t event)
{
  if (event == BUTTON_NONE) return UI_NO_ACTION;
  ui->dirty = 1;
  if (ui->screen == UI_WIFI_SCREEN) return event == BUTTON_DOUBLE ? UI_WIFI_EXIT : UI_NO_ACTION;
  if (ui->screen == UI_MERGING) return event == BUTTON_DOUBLE ? UI_CANCEL : UI_NO_ACTION;
  if (ui->screen == UI_CONFIRM)
  {
    ui->screen = UI_MENU;
    return event == BUTTON_LONG ? UI_MERGE : UI_NO_ACTION;
  }
  if (ui->screen == UI_PREVIEW)
  {
    if (event == BUTTON_LONG) return UI_RECORD;
    ui->item = 0;
    ui->screen = UI_MENU;
    return UI_NO_ACTION;
  }
  if (event == BUTTON_DOUBLE) { ui->screen = UI_PREVIEW; return UI_NO_ACTION; }
  if (event == BUTTON_CLICK) { ui->item = (uint8_t)((ui->item + 1) % 5); return UI_NO_ACTION; }
  if (ui->item == 4) { ui->screen = UI_CONFIRM; return UI_NO_ACTION; }
  return (UI_Action_t)(UI_INTERVAL + ui->item);
}
void UI_Draw(UI_t *ui, const UI_View_t *v)
{
  char text[40];
  if (ui->screen == UI_PREVIEW)
  {
    uint32_t minutes = v->duration_seconds / 60;
    if (minutes >= 60)
      snprintf(text, sizeof(text), "%s %02lu:%02lu:%02lu #%lu", v->recording ? "REC" : "READY",
               (unsigned long)(minutes / 60), (unsigned long)(minutes % 60),
               (unsigned long)(v->duration_seconds % 60), (unsigned long)v->frames);
    else
      snprintf(text, sizeof(text), "%s %02lu:%02lu #%lu", v->recording ? "REC" : "READY",
               (unsigned long)minutes, (unsigned long)(v->duration_seconds % 60), (unsigned long)v->frames);
    Display_OverlayLine(0, text, v->recording);
    Display_OverlayLine(5, v->message ? v->message : "CLICK:MENU HOLD:REC", 0);
  }
  else if (ui->screen == UI_MENU)
  {
    snprintf(text, sizeof(text), "INTERVAL: %s", v->interval);
    Display_Line(0, text, ui->item == 0);
    snprintf(text, sizeof(text), "SIZE: %ux%u", v->width, v->height);
    Display_Line(1, text, ui->item == 1);
    if (v->fragment_seconds < 60) snprintf(text, sizeof(text), "FRAGMENT: %u SEC", v->fragment_seconds);
    else snprintf(text, sizeof(text), "FRAGMENT: %u MIN", v->fragment_seconds / 60);
    Display_Line(2, text, ui->item == 2);
    Display_Line(3, "ESP WIFI: ENTER", ui->item == 3);
    Display_Line(4, "MERGE SESSION", ui->item == 4);
    Display_Line(5, v->message ? v->message : "HOLD:SET DOUBLE:BACK", 0);
  }
  else if (ui->screen == UI_WIFI_SCREEN)
  {
    Display_Line(0, v->wifi_ready ? "WIFI: ON" : "WAITING FOR ESP", 0);
    const char *password = v->wifi_password ? v->wifi_password : "";
    Display_Line(1, v->wifi_ssid && *v->wifi_ssid ? v->wifi_ssid : "READING ESP CONFIG...", 0);
    snprintf(text, sizeof(text), "PASS: %.20s", password);
    Display_Line(2, text, 0);
    Display_Line(3, strlen(password) > 20 ? password + 20 : "192.168.4.1", 0);
    Display_Line(4, strlen(password) > 46 ? password + 46 : (strlen(password) > 20 ? "192.168.4.1" : "VIDEO / DATE / TIME"), 0);
    Display_Line(5, "DOUBLE: WIFI OFF", 0);
  }
  else if (ui->screen == UI_CONFIRM)
  {
    Display_Clear();
    snprintf(text, sizeof(text), "MERGE R%07lu?", (unsigned long)v->session);
    Display_Line(1, text, 0);
    Display_Line(3, "HOLD: YES", 0);
    Display_Line(4, "CLICK/DOUBLE: CANCEL", 0);
  }
  else
  {
    snprintf(text, sizeof(text), "MERGING: %lu%%", (unsigned long)v->percent);
    Display_Line(1, text, 0);
    Display_Line(4, "DOUBLE: CANCEL", 0);
  }
  ui->dirty = 0;
}
