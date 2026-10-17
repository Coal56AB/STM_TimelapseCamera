#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
uint32_t millis();
void delay(unsigned ms);
struct TestSerial
{
  std::string rx;
  int reply_mode = 0;
  unsigned quota = 64;
  bool ended = false;
  bool allocationFailed = false, unavailable = false;
  unsigned rxSize = 0, txSize = 0, baud = 0, txTimeout = 0;
  void end() { ended = true; }
  size_t setRxBufferSize(unsigned size) { rxSize = size; return allocationFailed ? 0 : size; }
  size_t setTxBufferSize(unsigned size) { txSize = size; return allocationFailed ? 0 : size; }
  void begin(unsigned value) { baud = value; }
  void setTxTimeoutMs(unsigned value) { txTimeout = value; }
  int available() { return unavailable ? -1 : (int)(rx.size() < quota ? rx.size() : quota); }
  int read() { --quota; char c = rx[0]; rx.erase(0, 1); return c; }
  size_t read(uint8_t *buffer, size_t size) {
    size_t count = (size_t)available(); if (count > size) count = size;
    rx.copy((char *)buffer, count); rx.erase(0, count); quota -= count; return count;
  }
  size_t write(const uint8_t *data, size_t size);
};
extern TestSerial Serial;
#endif
