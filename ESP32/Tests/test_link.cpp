#include "Arduino.h"
#include "TimeLink.h"
#include <cassert>
#include <cstdio>
TestSerial Serial;
static TimeLink link;
static uint32_t tick;
uint32_t millis() { return tick; }
void delay(unsigned ms) { tick += ms; }
size_t TestSerial::write(const uint8_t *data, size_t size)
{
  std::string command((const char *)data, size);
  unsigned long id;
  if (sscanf(command.c_str(), "TIME %lu", &id) == 1)
  {
    if (reply_mode == 1) rx += "OK " + std::to_string(id) + "\n";
    if (reply_mode == 2) rx += "ERR " + std::to_string(id) + "\n";
    if (reply_mode == 3) rx += "OK " + std::to_string(id + 1) + "\n";
    if (reply_mode == 4) rx += "WIFI 0\n";
  }
  return size;
}
static void Enable()
{
  Serial.rx = "WIFI 1\n";
  link.update(false);
  assert(link.wifiRequested());
}
int main()
{
  link.begin();
  assert(!link.wifiRequested());
  assert(!link.setTime(946684800, 180));
  Serial.rx = "WIFI "; link.update(false);
  assert(!link.wifiRequested());
  Serial.rx = "1\r\n"; link.update(false);
  assert(link.wifiRequested());
  Serial.reply_mode = 1;
  assert(link.setTime(946684800, 180));
  assert(!link.setTime(946684799, 180));
  assert(!link.setTime(946684800, 841));
  Serial.reply_mode = 2; assert(!link.setTime(946684800, 180));
  Serial.reply_mode = 3; assert(!link.setTime(946684800, 180));
  assert(!link.wifiRequested());
  Enable(); Serial.reply_mode = 4; assert(!link.setTime(946684800, 180));
  assert(!link.wifiRequested());
  Serial.rx = std::string(110, 'x') + "WIFI 1\n";
  link.update(false); assert(!link.wifiRequested());
  tick = UINT32_MAX - 500; Enable();
  tick += 4999; link.update(true); assert(link.wifiRequested());
  tick += 1; link.update(true); assert(!link.wifiRequested());
  Enable(); Serial.rx = "WIFI 0 bad\n"; link.update(true);
  assert(link.wifiRequested());
  Serial.rx = "WIFI 0\n"; link.update(true); assert(!link.wifiRequested());
  puts("PASS: ESP USB framing, Wi-Fi off/start/stop/lease, timestamp ACK/error/timeout, tick wrap");
}
