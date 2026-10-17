#include "Arduino.h"
#include "CameraLink.h"
#include "Config.h"
#include <cassert>
#include <cstdio>
#include <cstring>
TestSerial Serial;
static CameraLink link;
static uint32_t tick;
static bool ssidSent, passwordSent;
static bool statusSent;
static bool transferSent;
uint32_t millis() { return tick; }
void delay(unsigned ms) { tick += ms; Serial.quota = 64; }
size_t TestSerial::write(const uint8_t *data, size_t size)
{
  std::string command((const char *)data, size);
  if (command == std::string("APSSID ") + WIFI_SSID + "\n") ssidSent = true;
  if (command == std::string("APPASS ") + WIFI_PASSWORD + "\n") passwordSent = true;
  if (command == "APINFO STARTING\n") statusSent = true;
  if (command == "APINFO USB 20K HTTP 800K\n") transferSent = true;
  unsigned long id;
  if (sscanf(command.c_str(), "TIME %lu", &id) == 1)
  {
    if (reply_mode == 1) rx += "OK " + std::to_string(id) + "\n";
    if (reply_mode == 2) rx += "ERR " + std::to_string(id) + "\n";
    if (reply_mode == 3) rx += "OK " + std::to_string(id + 1) + "\n";
    if (reply_mode == 4) rx += "WIFI 0\n";
  }
  if (sscanf(command.c_str(), "FILES %lu", &id) == 1)
  {
    if (reply_mode == 5) {
      std::string bytes(16384, '\0');
      for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = (char)(i % 256);
      bytes.replace(100, 7, "WIFI 0\n");
      rx += "FILE " + std::to_string(id) + " 0 7 7000000000 16384\n" + bytes;
    }
    if (reply_mode == 6) rx += "FILE " + std::to_string(id) + " 3 7 0 0\n";
    if (reply_mode == 7) rx += "FILE " + std::to_string(id - 1) + " 0 7 8 8\nWIFI 0\nXFILE " + std::to_string(id) + " 0 8 0 0\n";
    if (reply_mode == 8) rx += "FILE " + std::to_string(id) + " 0 7 7000000000 16384\nshort";
    if (reply_mode == 9) rx += "FILE " + std::to_string(id) + " 0 7 7000000000 0 2\n";
  }
  return size;
}
static void Enable()
{
  Serial.rx = "WIFI 1\n";
  Serial.quota = 64;
  link.update(false);
  assert(link.wifiRequested());
}
int main()
{
  Serial.allocationFailed = true; UsbSerial failed; uint8_t byte;
  assert(!failed.begin() && !failed.available() && failed.read() == -1);
  assert(!failed.read(&byte, 1) && !failed.write(&byte, 1)); failed.wait(); Serial.allocationFailed = false;
  link.begin();
  assert(!Serial.ended && Serial.rxSize == 32768 && Serial.txSize == 2048 && Serial.baud == 115200 && Serial.txTimeout == 50);
  Serial.unavailable = true; link.update(false, "STARTING"); assert(!link.wifiRequested()); Serial.unavailable = false;
  assert(!link.wifiRequested());
  link.update(false, "STARTING"); assert(ssidSent && passwordSent && statusSent);
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
  for (unsigned i = 0; i < 3; ++i) { delay(1); link.update(false); }
  assert(!link.wifiRequested());
  tick = UINT32_MAX - 500; Enable();
  tick += 4999; link.update(true); assert(link.wifiRequested());
  tick += 1; link.update(true); assert(!link.wifiRequested());
  Enable(); Serial.rx = "WIFI 0 bad\n"; link.update(true);
  assert(link.wifiRequested());
  Serial.rx = "WIFI 0\n"; link.update(true); assert(!link.wifiRequested());
  Enable(); FileReply reply; uint8_t payload[16384];
  Serial.reply_mode = 5;
  assert(link.fileCommand("READ 7 0", payload, sizeof(payload), reply));
  assert(reply.size == 7000000000ULL && reply.length == sizeof(payload) && reply.token == 7);
  assert(!memcmp(payload + 100, "WIFI 0\n", 7) && link.wifiRequested());
  for (unsigned i = 0; i < sizeof(payload); ++i)
    if (i < 100 || i >= 107) assert(payload[i] == i % 256);
  for (unsigned quota : {1U, 63U, 65U, 1024U, 4096U}) {
    Serial.quota = quota;
    assert(link.fileCommand("READ 7 0", payload, sizeof(payload), reply));
    for (unsigned i = 0; i < sizeof(payload); ++i)
      if (i < 100 || i >= 107) assert(payload[i] == i % 256);
  }
  link.reportTransfer("USB 20K HTTP 800K"); assert(transferSent);
  Serial.reply_mode = 6; assert(!link.fileCommand("OPEN 9 0", nullptr, 0, reply));
  Serial.reply_mode = 7; assert(link.fileCommand("CLOSE 7", nullptr, 0, reply));
  assert(reply.token == 8 && link.wifiRequested());
  Serial.reply_mode = 8; assert(!link.fileCommand("READ 7 0", payload, sizeof(payload), reply));
  assert(!link.wifiRequested());
  Enable(); Serial.reply_mode = 5;
  assert(link.fileCommand("READ 7 0", payload, sizeof(payload), reply));
  puts("PASS: ESP USB framing, Wi-Fi off/start/stop/lease, timestamp ACK/error/timeout, tick wrap");
  puts("PASS: binary file blocks, 64-bit file size, stale replies, errors and truncated packet timeout");
  Serial.reply_mode = 9; assert(link.fileCommand("OPEN 1 0", payload, sizeof(payload), reply) && reply.codec == 2);
  assert(!Serial.ended);
}
