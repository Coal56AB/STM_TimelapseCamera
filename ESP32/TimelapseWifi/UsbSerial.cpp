#include "UsbSerial.h"
#include <Arduino.h>

bool UsbSerial::begin() {
  if (ready_) return true;
  // Keep Arduino HWCDC as the sole owner of the peripheral and USB pins.
  size_t rx = Serial.setRxBufferSize(32768);
  size_t tx = Serial.setTxBufferSize(2048);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(50);
  ready_ = rx == 32768 && tx == 2048;
  return ready_;
}
size_t UsbSerial::available() {
  if (!ready_) return 0;
  int count = Serial.available();
  return count > 0 ? (size_t)count : 0;
}
int UsbSerial::read() { return available() ? Serial.read() : -1; }
size_t UsbSerial::read(uint8_t *data, size_t size) {
  if (!data || !size) return 0;
  size_t count = available();
  if (count > size) count = size;
  if (!count) return 0;
  size_t received = Serial.read(data, count);
  return received <= count ? received : 0;
}
size_t UsbSerial::write(const uint8_t *data, size_t size) {
  if (!ready_ || !data || !size) return 0;
  return Serial.write(data, size);
}
void UsbSerial::wait() {
  if (!available()) delay(1);
}
