#pragma once
#include <stdint.h>
#include <stddef.h>

class UsbSerial {
 public:
  bool begin();
  size_t available();
  int read();
  size_t read(uint8_t *data, size_t size);
  size_t write(const uint8_t *data, size_t size);
  void wait();
 private:
  bool ready_ = false;
  uint8_t buffered_[1024];
  size_t position_ = 0, length_ = 0;
};
