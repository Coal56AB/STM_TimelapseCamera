#include "button.h"
#include "recording.h"
#include "timelapse_config.h"
#include "ui.h"
#include "display.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void Host_Format(void);
extern int Host_FailWrite;
extern int Host_Present;
extern BYTE Host_FormatType;
static uint8_t pixels[320 * 240 * 2], output[700000];
static Button_t button;
static uint32_t tick;
static unsigned events[4];
static void Hold(uint8_t pressed, uint32_t ms)
{
  for (uint32_t i = 0; i < ms; ++i) ++events[Button_Process(&button, pressed, tick++)];
}
static void ButtonTests(void)
{
  const Button_Config_t config = {25, 300, 800};
  Button_Init(&button, &config); tick = UINT32_MAX - 40;
  Hold(0, 10); Hold(1, 5); Hold(0, 5); Hold(1, 80); Hold(0, 400);
  assert(events[BUTTON_CLICK] == 1 && !events[BUTTON_DOUBLE] && !events[BUTTON_LONG]);
  Hold(1, 100); Hold(0, 100); Hold(1, 100); Hold(0, 400);
  assert(events[BUTTON_CLICK] == 1 && events[BUTTON_DOUBLE] == 1);
  Hold(1, 1500); Hold(0, 400); assert(events[BUTTON_LONG] == 1 && events[BUTTON_CLICK] == 1);
  UI_t ui; UI_Init(&ui);
  assert(UI_Process(&ui, BUTTON_LONG) == UI_RECORD && ui.screen == UI_PREVIEW);
  assert(UI_Process(&ui, BUTTON_CLICK) == UI_NO_ACTION && ui.screen == UI_MENU);
  assert(UI_Process(&ui, BUTTON_LONG) == UI_INTERVAL);
  UI_Process(&ui, BUTTON_CLICK); UI_Process(&ui, BUTTON_CLICK);
  assert(UI_Process(&ui, BUTTON_LONG) == UI_FRAGMENT);
  UI_Process(&ui, BUTTON_CLICK);
  assert(UI_Process(&ui, BUTTON_LONG) == UI_WIFI);
  ui.screen = UI_WIFI_SCREEN;
  assert(UI_Process(&ui, BUTTON_CLICK) == UI_NO_ACTION);
  assert(UI_Process(&ui, BUTTON_LONG) == UI_NO_ACTION);
  assert(UI_Process(&ui, BUTTON_DOUBLE) == UI_WIFI_EXIT);
  ui.screen = UI_MENU;
  UI_Process(&ui, BUTTON_DOUBLE); UI_Process(&ui, BUTTON_CLICK);
  assert(ui.screen == UI_MENU && ui.item == 0);
  for (unsigned i = 0; i < 4; ++i) UI_Process(&ui, BUTTON_CLICK);
  UI_Process(&ui, BUTTON_LONG); assert(ui.screen == UI_CONFIRM);
  assert(UI_Process(&ui, BUTTON_DOUBLE) == UI_NO_ACTION && ui.screen == UI_MENU);
  UI_Process(&ui, BUTTON_LONG); assert(UI_Process(&ui, BUTTON_LONG) == UI_MERGE);
  ui.screen = UI_MERGING; assert(UI_Process(&ui, BUTTON_DOUBLE) == UI_CANCEL);
}
void Display_Clear(void) { }
void Display_Line(uint8_t row, const char *text, uint8_t selected) { (void)row; (void)text; (void)selected; }
static char status_line[40];
void Display_OverlayLine(uint8_t row, const char *text, uint8_t highlighted)
{
  (void)highlighted;
  if (row == 0) snprintf(status_line, sizeof(status_line), "%s", text);
}
static void StatusTests(void)
{
  UI_t ui; UI_Init(&ui);
  UI_View_t view = {0}; view.recording = 1; view.duration_seconds = 3; view.frames = 75;
  UI_Draw(&ui, &view); assert(!strcmp(status_line, "REC 00:03 #75"));
  view.duration_seconds = 301; view.frames = 7525;
  UI_Draw(&ui, &view); assert(!strcmp(status_line, "REC 05:01 #7525"));
  view.duration_seconds = 3661; view.frames = 91525;
  UI_Draw(&ui, &view); assert(!strcmp(status_line, "REC 01:01:01 #91525"));
}
static void Fresh(void)
{
  Host_Format(); assert(Store_Init() == STORE_OK); assert(Recording_Init() == STORE_OK);
  if (Host_FormatType == FM_EXFAT)
  {
    uint64_t free_bytes;
    assert(Store_Free(&free_bytes) == STORE_OK && free_bytes > 60000000000ULL);
  }
}
static void ReadFile(const char *path, uint8_t *data, uint32_t size)
{
  Store_File_t file = {0}; assert(Store_Open(&file, path, STORE_READ) == STORE_OK);
  assert(Store_Read(&file, data, size) == STORE_OK); assert(Store_Close(&file) == STORE_OK);
}
static void SettingsTests(void)
{
  Timelapse_Config_t cfg, loaded;
  Fresh(); assert(Timelapse_ConfigLoad(&cfg) == STORE_OK && cfg.resolution == 1 && cfg.fragment == 3);
  cfg.interval = 3; cfg.fragment = 1; assert(Timelapse_ConfigSave(&cfg) == STORE_OK);
  cfg.interval = 5; cfg.fragment = 2; assert(Timelapse_ConfigSave(&cfg) == STORE_OK);
  assert(Timelapse_ConfigLoad(&loaded) == STORE_OK && loaded.interval == 5 && loaded.fragment == 2);
  assert(Store_Remove("0:/TL/SET0.BIN") == STORE_OK);
  Store_File_t file = {0}; assert(Store_Open(&file, "0:/TL/SET0.BIN", STORE_CREATE) == STORE_OK);
  assert(Store_Write(&file, "broken", 6) == STORE_OK); assert(Store_Close(&file) == STORE_OK);
  assert(Timelapse_ConfigLoad(&loaded) == STORE_OK && loaded.interval == 3 && loaded.generation == 1 && loaded.fragment == 1);
}
static void FragmentTests(void)
{
  /* Маленький кадр сохраняет реальное число кадров и уменьшает объём теста. */
  AVI_Format_t format = {4, 2, 25}, parsed;
  const uint16_t seconds[] = {30, 60, 120, 300};
  for (unsigned option = 0; option < 4; ++option)
  {
    Fresh(); Recording_t r = {0}; Recording_Stream_t stream; Recording_Merge_t merge = {0};
    uint32_t limit = (uint32_t)seconds[option] * 25, size, count, read, total = 0;
    assert(Timelapse_Fragments[option] == seconds[option]);
    assert(Recording_Start(&r, &format, seconds[option]) == STORE_OK);
    for (uint32_t i = 0; i < limit - 1; ++i) assert(Recording_Append(&r, pixels, NULL) == STORE_OK);
    assert(Store_Stat("0:/TL/R0000001/F0000001.AVI", &size) == STORE_NOT_FOUND);
    assert(Recording_Append(&r, pixels, NULL) == STORE_OK && r.active && r.fragment == 2 && !r.fragment_frames);
    assert(Store_Stat("0:/TL/R0000001/F0000001.AVI", &size) == STORE_OK && size == AVI_Size(&format, limit));
    ReadFile("0:/TL/R0000001/F0000001.AVI", output, AVI_HEADER_SIZE);
    assert(AVI_Parse(output, &parsed, &count) && count == limit);
    for (unsigned i = 0; i < 3; ++i) assert(Recording_Append(&r, pixels, NULL) == STORE_OK);
    assert(Recording_Stop(&r) == STORE_OK);
    assert(Store_Stat("0:/TL/R0000001/F0000002.AVI", &size) == STORE_OK && size == AVI_Size(&format, 3));
    assert(Recording_StreamOpen(&stream, 1) == STORE_OK && stream.frames == limit + 3);
    do
    {
      assert(Recording_StreamRead(&stream, output + total, 257, &read) == STORE_OK); total += read;
    } while (read);
    Recording_StreamClose(&stream);
    assert(total == AVI_Size(&format, limit + 3));
    assert(AVI_Parse(output, &parsed, &count) && count == limit + 3);
    uint32_t index_at = AVI_HEADER_SIZE + count * (AVI_FrameBytes(&format) + 8);
    for (uint32_t i = 0; i < count; ++i)
    {
      uint8_t expected[16]; AVI_IndexEntry(expected, &format, i);
      assert(!memcmp(expected, output + index_at + 8 + i * 16, 16));
    }
    assert(Recording_MergeStart(&merge, 1) == STORE_OK);
    while (merge.active) assert(Recording_MergeProcess(&merge) == STORE_OK);
    Store_File_t file = {0}; uint8_t block[257];
    assert(Store_Open(&file, "0:/TL/R0000001/ALL.AVI", STORE_READ) == STORE_OK);
    for (uint32_t at = 0; at < total; )
    {
      uint32_t n = total - at < sizeof(block) ? total - at : sizeof(block);
      assert(Store_Read(&file, block, n) == STORE_OK && !memcmp(block, output + at, n)); at += n;
    }
    assert(Store_Close(&file) == STORE_OK);
  }
}
static void HotplugTests(void)
{
  Fresh(); Host_Present = 0;
  assert(Store_Init() != STORE_OK);
  Host_Present = 1; assert(Store_Init() == STORE_OK && Recording_Init() == STORE_OK);
  Recording_t r = {0}; AVI_Format_t format = {4, 2, 25}; uint32_t size;
  assert(Recording_Start(&r, &format, 30) == STORE_OK);
  for (unsigned i = 0; i < 750; ++i) assert(Recording_Append(&r, pixels, NULL) == STORE_OK);
  assert(Recording_Append(&r, pixels, NULL) == STORE_OK);
  Store_File_t stale = r.file;
  Host_Present = 0; assert(Store_Check() != STORE_OK);
  Store_Reset(); Recording_Abort(&r);
  assert(!r.active && !r.file.handle);
  Host_Present = 1; assert(Store_Init() == STORE_OK && Store_Check() == STORE_OK);
  assert(Store_Stat("0:/TL/R0000001/F0000001.AVI", &size) == STORE_OK && size == AVI_Size(&format, 750));
  assert(Recording_Start(&r, &format, 30) == STORE_OK && r.id == 2);
  assert(Recording_Append(&r, pixels, NULL) == STORE_OK);
  assert(Store_Write(&stale, "bad", 3) == STORE_IO && Store_Close(&stale) == STORE_IO);
  assert(Recording_Stop(&r) == STORE_OK);
  Recording_Stream_t stream;
  assert(Recording_StreamOpen(&stream, 1) == STORE_OK && stream.frames == 750);
  Recording_StreamClose(&stream);
  assert(Recording_StreamOpen(&stream, 2) == STORE_OK && stream.frames == 1);
  Recording_StreamClose(&stream);
}
static void RecordingTests(void)
{
  AVI_Format_t format = {320, 240, 25}; Recording_t r = {0}; Recording_Merge_t merge = {0};
  Recording_Stream_t stream; uint32_t read, total = 0, size;
  Fresh();
  for (unsigned y = 0; y < 240; ++y) for (unsigned x = 0; x < 320; ++x)
  {
    uint16_t p = y < 120 ? 0xf800 : 0x07ff;
    pixels[(y * 320 + x) * 2] = (uint8_t)(p >> 8); pixels[(y * 320 + x) * 2 + 1] = (uint8_t)p;
  }
  assert(Recording_Start(&r, &format, 30) == STORE_OK && r.id == 1);
  for (unsigned i = 0; i < 3; ++i) assert(Recording_Append(&r, pixels, NULL) == STORE_OK);
  Recording_Stop(&r);
  assert(Store_Stat("0:/TL/R0000001/F0000001.AVI", &size) == STORE_OK && size == 691504);
  assert(Recording_StreamOpen(&stream, 1) == STORE_OK);
  do
  {
    assert(Recording_StreamRead(&stream, output + total, (total % 503) + 1, &read) == STORE_OK); total += read;
  } while (read);
  assert(total == 691504); Recording_StreamClose(&stream);
  assert(Recording_MergeStart(&merge, 1) == STORE_OK); assert(Recording_MergeProcess(&merge) == STORE_OK);
  Recording_MergeCancel(&merge);
  assert(Store_Stat("0:/TL/R0000001/ALL.AVI", &size) == STORE_NOT_FOUND);
  assert(Store_Stat("0:/TL/R0000001/ALL.TMP", &size) == STORE_NOT_FOUND);
  assert(Recording_MergeStart(&merge, 1) == STORE_OK);
  while (merge.active) assert(Recording_MergeProcess(&merge) == STORE_OK);
  assert(Store_Stat("0:/TL/R0000001/ALL.AVI", &size) == STORE_OK && size == total);
  Store_File_t file = {0}; uint8_t block[257]; uint32_t at = 0;
  assert(Store_Open(&file, "0:/TL/R0000001/ALL.AVI", STORE_READ) == STORE_OK);
  while (at < size)
  {
    uint32_t n = size - at < sizeof(block) ? size - at : sizeof(block);
    assert(Store_Read(&file, block, n) == STORE_OK); assert(!memcmp(block, output + at, n)); at += n;
  }
  Store_Close(&file); assert(Recording_MergeStart(&merge, 1) == STORE_EXISTS);
  FILE *exported = fopen("merged.avi", "wb"); assert(exported);
  assert(fwrite(output, 1, total, exported) == total); fclose(exported);
  assert(Store_Init() == STORE_OK); assert(Recording_Start(&r, &format, 30) == STORE_OK && r.id == 2);
  Recording_Stop(&r);
  uint32_t selected; assert(Recording_Select(1, &selected) == STORE_OK && selected == 2);
  assert(Recording_Select(2, &selected) == STORE_OK && selected == 1);
  assert(Recording_Latest(&selected) == STORE_OK && selected == 2);
  assert(Store_Rename("0:/TL/R0000001/F0000001.AVI", "0:/TL/R0000001/F0000002.AVI") == STORE_OK);
  assert(Recording_StreamOpen(&stream, 1) == STORE_IO); assert(!AVI_Size(&format, 100000));
}
static void TimestampTests(void)
{
  Fresh();
  AVI_Format_t format = {160, 120, 25};
  Recording_t r = {0};
  memset(pixels, 0x55, 160 * 120 * 2);
  assert(Recording_Start(&r, &format, 30) == STORE_OK);
  assert(Recording_Append(&r, pixels, "15.10.2026 12:34:56") == STORE_OK);
  assert(Recording_Stop(&r) == STORE_OK);
  uint32_t size = AVI_Size(&format, 1);
  ReadFile("0:/TL/R0000001/F0000001.AVI", output, size);
  uint8_t original[480]; AVI_ConvertRow(original, pixels, 160);
  unsigned changed = 0;
  for (unsigned y = 0; y < 120; ++y)
    for (unsigned x = 0; x < 160; ++x)
    {
      const uint8_t *pixel = output + AVI_HEADER_SIZE + 8 + (119 - y) * 480 + x * 3;
      if (memcmp(pixel, original + x * 3, 3))
      {
        assert(y >= 105 && y < 118 && x >= 2 && x < 117);
        assert((pixel[0] == 0 || pixel[0] == 255) && pixel[0] == pixel[1] && pixel[1] == pixel[2]);
        ++changed;
      }
    }
  assert(changed > 200);
  for (unsigned i = 0; i < 160 * 120 * 2; ++i) assert(pixels[i] == 0x55);
}

static void FailureTests(void)
{
  AVI_Format_t format = {160, 120, 25}; uint8_t original[57856], actual[57856];
  for (int fail_at = 0; fail_at < 40; ++fail_at)
  {
    Recording_t r = {0}; Fresh(); assert(Recording_Start(&r, &format, 30) == STORE_OK);
    assert(Recording_Append(&r, pixels, NULL) == STORE_OK);
    assert(Recording_Stop(&r) == STORE_OK);
    ReadFile("0:/TL/R0000001/F0000001.AVI", original, sizeof(original));
    assert(Recording_Start(&r, &format, 30) == STORE_OK);
    Host_FailWrite = fail_at; Store_Result_t result = Recording_Append(&r, pixels, NULL); Host_FailWrite = -1;
    assert(result != STORE_OK && !r.active && r.frames == 0);
    assert(Store_Init() == STORE_OK); ReadFile("0:/TL/R0000001/F0000001.AVI", actual, sizeof(actual));
    assert(!memcmp(actual, original, sizeof(actual)));
    Recording_Stream_t stream; assert(Recording_StreamOpen(&stream, 1) == STORE_OK && stream.frames == 1);
    Recording_StreamClose(&stream);
  }
}
int main(void)
{
  ButtonTests();
  StatusTests();
  const BYTE formats[] = {FM_FAT, FM_FAT32, FM_EXFAT};
  const char *names[] = {"FAT16", "FAT32", "exFAT 64 GB"};
  for (unsigned i = 0; i < sizeof(formats); ++i)
  {
    Host_FormatType = formats[i];
    SettingsTests(); RecordingTests(); FragmentTests(); HotplugTests(); FailureTests(); TimestampTests();
    printf("PASS: %s: settings, 4 fragment durations, multi-fragment merge, hotplug, 40 write failures\n", names[i]);
  }
  return 0;
}
