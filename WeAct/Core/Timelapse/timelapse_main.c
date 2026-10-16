/** @brief Прикладной слой: камера, расписание, запись и команды меню. */
#include "timelapse_main.h"
#include "timelapse_config.h"
#include "camera.h"
#include "recording.h"
#include "ui.h"
#include "display.h"
#include "board.h"
#include "clock.h"
#include "esp_control.h"
#include "media_memory.h"

typedef struct
{
  Timelapse_Config_t config;
  Recording_t recording;
  Recording_Merge_t merge;
  UI_t ui;
  uint32_t next_capture, next_sd_check, last_ui, selected, message_at;
  uint8_t sd_ready, camera_ready, capture_pending, save_frame;
  const char *message, *camera_message;
  Clock_t clock;
  char frame_time[20];
  uint8_t esp_wifi;
  uint16_t capture_fraction;
} Timelapse_t;
static Timelapse_t app;

static void CameraStatus(Camera_Result_t result)
{
  app.camera_ready = result == CAMERA_OK;
  switch (result)
  {
    case CAMERA_OK: app.camera_message = NULL; break;
    case CAMERA_NOT_FOUND: app.camera_message = "CAMERA NOT FOUND"; break;
    case CAMERA_UNSUPPORTED: app.camera_message = "OV5640: STUB"; break;
    case CAMERA_TIMEOUT: app.camera_message = "CAMERA FRAME TIMEOUT"; break;
    case CAMERA_OVERRUN: app.camera_message = "CAMERA DCMI OVERRUN"; break;
    case CAMERA_DMA_ERROR: app.camera_message = "CAMERA DMA ERROR"; break;
    case CAMERA_SYNC_ERROR: app.camera_message = "CAMERA SYNC ERROR"; break;
    default: app.camera_message = "CAMERA INIT ERROR"; break;
  }
}

static void Message(const char *text)
{
  app.message = text;
  app.message_at = Board_Time();
  app.ui.dirty = 1;
}
static void StorageError(Store_Result_t result)
{
  switch (result)
  {
    case STORE_FULL: Message("NO SPACE / FILE LIMIT"); break;
    case STORE_EXISTS: Message("MERGED FILE EXISTS"); break;
    case STORE_NOT_FOUND: Message("NO RECORDING FOUND"); break;
    default: Message("SD ERROR"); break;
  }
}
static int Mount(void)
{
  Store_Result_t result = Store_Init();
  if (result == STORE_OK) result = Recording_Init();
  app.sd_ready = result == STORE_OK;
  if (!app.sd_ready) StorageError(result);
  else
  {
    Timelapse_Config_t loaded;
    if (Timelapse_ConfigLoad(&loaded) == STORE_OK)
    {
      uint8_t resize = loaded.resolution != app.config.resolution;
      app.config = loaded;
      if (resize && app.camera_ready)
      {
        Camera_Stop(); app.capture_pending = 0;
        CameraStatus(Camera_Init(Timelapse_Resolutions[loaded.resolution][0], Timelapse_Resolutions[loaded.resolution][1]));
      }
    }
    app.selected = 0;
    Recording_Latest(&app.selected);
  }
  app.next_sd_check = Board_Time() + 1000;
  return app.sd_ready;
}
static void Stop(void)
{
  Store_Result_t result = Recording_Stop(&app.recording);
  app.save_frame = 0;
  Board_Led(0);
  if (result != STORE_OK) StorageError(result);
  else Message("RECORDING CLOSED");
}
static void CardLost(void)
{
  /* Никаких f_close/f_sync после смены носителя: старые дескрипторы отзываются. */
  Store_Reset();
  Recording_Abort(&app.recording);
  app.merge = (Recording_Merge_t){0};
  app.sd_ready = 0; app.save_frame = 0; app.selected = 0;
  app.next_sd_check = Board_Time() + 1000;
  if (app.ui.screen == UI_MERGING) app.ui.screen = UI_MENU;
  Board_Led(0); Message("SD DISCONNECTED");
}
static void Action(UI_Action_t action)
{
  Store_Result_t result;
  if (action == UI_NO_ACTION) return;
  if (action == UI_WIFI)
  {
    if (app.recording.active) Stop();
    Camera_Stop(); app.capture_pending = app.save_frame = 0;
    app.esp_wifi = 0;
    app.ui.screen = UI_WIFI_SCREEN;
    return;
  }
  if (action == UI_WIFI_EXIT)
  {
    app.ui.screen = UI_PREVIEW;
    return;
  }
  if (action == UI_CANCEL)
  {
    Recording_MergeCancel(&app.merge); app.ui.screen = UI_MENU; Message("MERGE CANCELLED"); return;
  }
  if (action == UI_RECORD && app.recording.active) { Stop(); return; }
  if (!app.sd_ready && !Mount()) return;
  if (action == UI_INTERVAL || action == UI_RESOLUTION || action == UI_FRAGMENT)
  {
    if (app.recording.active) { Message("STOP RECORDING FIRST"); return; }
    Timelapse_Config_t next = app.config;
    if (action == UI_INTERVAL) next.interval = (uint8_t)((next.interval + 1) % TIMELAPSE_INTERVAL_COUNT);
    else if (action == UI_RESOLUTION) next.resolution = (uint8_t)((next.resolution + 1) % TIMELAPSE_RESOLUTION_COUNT);
    else next.fragment = (uint8_t)((next.fragment + 1) % TIMELAPSE_FRAGMENT_COUNT);
    result = Timelapse_ConfigSave(&next);
    if (result != STORE_OK) { StorageError(result); return; }
    app.config = next;
    if (action == UI_RESOLUTION)
    {
      Camera_Stop(); app.capture_pending = 0;
      CameraStatus(Camera_Init(Timelapse_Resolutions[next.resolution][0], Timelapse_Resolutions[next.resolution][1]));
    }
    Message(app.camera_ready ? "SETTINGS SAVED" : app.camera_message);
  }
  else if (action == UI_RECORD)
  {
    if (!app.camera_ready) { Message(app.camera_message); return; }
    AVI_Format_t format = {Timelapse_Resolutions[app.config.resolution][0], Timelapse_Resolutions[app.config.resolution][1], TIMELAPSE_PLAYBACK_FPS, AVI_H264};
    result = Recording_Start(&app.recording, &format, Timelapse_Fragments[app.config.fragment]);
    if (result != STORE_OK) { StorageError(result); return; }
    Camera_Stop(); app.capture_pending = 0;
    app.selected = app.recording.id; app.next_capture = Board_Time(); app.capture_fraction = 0;
    Board_Led(1); Message("RECORDING STARTED");
  }
  else if (action == UI_MERGE)
  {
    if (app.recording.active) { Message("STOP RECORDING FIRST"); return; }
    Camera_Stop(); app.capture_pending = 0;
    result = Recording_MergeStart(&app.merge, app.selected);
    if (result != STORE_OK) { StorageError(result); return; }
    app.ui.screen = UI_MERGING;
    Display_Clear();
  }
}

void Timelapse_Init(void)
{
  Camera_SetBuffer(MEDIA_CAPTURE_BUFFER);
  UI_Init(&app.ui);
  Display_Init();
  app.config = (Timelapse_Config_t){3, 1, 0, 3};
  Mount();
  Camera_Result_t result = Camera_Init(Timelapse_Resolutions[app.config.resolution][0], Timelapse_Resolutions[app.config.resolution][1]);
  CameraStatus(result);
  if (!app.camera_ready) Message(app.camera_message);
}

void Timelapse_Process(void)
{
  uint32_t now = Board_Time();
  Clock_Update(&app.clock, now);
  app.esp_wifi = EspControl_Process(&app.clock, app.ui.screen == UI_WIFI_SCREEN, app.sd_ready, now);
  if ((int32_t)(now - app.next_sd_check) >= 0)
  {
    if (app.sd_ready)
    {
      if (Store_Check() != STORE_OK) CardLost();
    }
    else if (Mount()) Message("SD READY");
    app.next_sd_check = Board_Time() + 1000;
  }
  Button_Event_t event;
  while ((event = Board_Button()) != BUTTON_NONE)
  {
    UI_Screen_t before = app.ui.screen;
    Action(UI_Process(&app.ui, event));
    if (before != app.ui.screen) Display_Clear();
  }
  if (app.merge.active)
  {
    Store_Result_t result = Recording_MergeProcess(&app.merge);
    if (result != STORE_OK || !app.merge.active)
    {
      app.ui.screen = UI_MENU;
      if (result == STORE_OK) Message(app.merge.stream.format.codec == AVI_H264 ? "SAVED: ALL.MP4" : "SAVED: ALL.AVI");
      else StorageError(result);
    }
  }
  else if (app.camera_ready)
  {
    if (app.capture_pending)
    {
      Camera_Frame_t frame;
      Camera_Result_t result = Camera_Process(Board_Time(), &frame);
      if (result == CAMERA_OK)
      {
        app.capture_pending = 0;
        if (app.ui.screen == UI_PREVIEW)
        {
          Display_Preview(&frame);
        }
        if (app.save_frame && app.recording.active)
        {
          Store_Result_t stored = Recording_Append(&app.recording, frame.rgb565_be, app.frame_time);
          if (stored != STORE_OK) { CardLost(); StorageError(stored); }
          app.ui.dirty = 1;
        }
        app.save_frame = 0;
      }
      else if (result != CAMERA_BUSY)
      {
        app.capture_pending = 0; CameraStatus(result); Stop(); Message(app.camera_message);
      }
    }
    now = Board_Time();
    uint8_t due = app.recording.active && (int32_t)(now - app.next_capture) >= 0;
    uint8_t preview = app.ui.screen == UI_PREVIEW;
    if (!app.capture_pending && (due || preview))
    {
      Clock_Update(&app.clock, now);
      if (due) Clock_Format(&app.clock, app.frame_time);
      Camera_Result_t result = Camera_Start(now);
      if (result == CAMERA_OK)
      {
        app.capture_pending = 1; app.save_frame = due;
        /* Пропущенные интервалы не порождают серию кадров без временного промежутка. */
        if (due) app.next_capture = Timelapse_Deadline(app.config.interval, app.next_capture, now, &app.capture_fraction);
      }
      else { CameraStatus(result); Stop(); Message(app.camera_message); }
    }
  }
  now = Board_Time();
  if (app.message && now - app.message_at > 4000) { app.message = NULL; app.ui.dirty = 1; }
  if (app.ui.dirty || now - app.last_ui >= 250)
  {
    const char *message = app.message;
    if (!message && !app.camera_ready) message = app.camera_message;
    if (!message && !app.sd_ready) message = "SD NOT READY";
    UI_View_t view = {Timelapse_Intervals[app.config.interval], app.selected, app.recording.frames,
      app.merge.active ? (uint32_t)((uint64_t)app.merge.stream.position * 100 / app.merge.stream.size) : 0,
      Timelapse_Resolutions[app.config.resolution][0], Timelapse_Resolutions[app.config.resolution][1],
      app.recording.active, message, Timelapse_Fragments[app.config.fragment],
      app.recording.frames / TIMELAPSE_PLAYBACK_FPS,
      app.esp_wifi, EspControl_Ssid(), EspControl_Password()};
    UI_Draw(&app.ui, &view);
    app.last_ui = Board_Time();
  }
}
