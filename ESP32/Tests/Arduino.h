#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H
#include <stdint.h>
#include <stddef.h>
#include <string>
uint32_t millis();
void delay(unsigned ms);
struct TestSerial
{
  std::string rx;
  int reply_mode = 0;
  void setRxBufferSize(unsigned) {}
  void begin(unsigned) {}
  void setTxTimeoutMs(unsigned) {}
  int available() { return !rx.empty(); }
  int read() { char c = rx[0]; rx.erase(0, 1); return c; }
  size_t write(const uint8_t *data, size_t size);
};
extern TestSerial Serial;
#endif
