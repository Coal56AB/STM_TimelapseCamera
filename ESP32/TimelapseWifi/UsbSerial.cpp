#include "UsbSerial.h"
#include <Arduino.h>
#include <driver/usb_serial_jtag.h>
#include <hal/usb_serial_jtag_ll.h>
#include <string.h>

bool UsbSerial::begin() {
  if (ready_) return true;
  // Arduino enables HWCDC at boot; only one driver may own the USB interrupt.
  Serial.end();
  usb_serial_jtag_driver_config_t config = {2048, 32768};
  ready_ = usb_serial_jtag_driver_install(&config) == ESP_OK;
  // HWCDC.end() clears TX-empty. Prime a ZLP so the new driver's first write
  // can receive TX-empty after the host polls the IN endpoint.
  if (ready_) usb_serial_jtag_ll_txfifo_flush();
  return ready_;
}
size_t UsbSerial::available() {
  if (!ready_) return 0;
  if (position_ == length_) {
    int count = usb_serial_jtag_read_bytes(buffered_, sizeof(buffered_), 0);
    position_ = 0; length_ = count > 0 ? (size_t)count : 0;
  }
  return length_ - position_;
}
int UsbSerial::read() { return available() ? buffered_[position_++] : -1; }
size_t UsbSerial::read(uint8_t *data, size_t size) {
  if (!data || !size) return 0;
  size_t count = available();
  if (count > size) count = size;
  memcpy(data, buffered_ + position_, count); position_ += count;
  return count;
}
size_t UsbSerial::write(const uint8_t *data, size_t size) {
  if (!ready_ || !data || !size) return 0;
  int count = usb_serial_jtag_write_bytes(data, size, pdMS_TO_TICKS(50));
  return count > 0 ? (size_t)count : 0;
}
void UsbSerial::wait() {
  if (!ready_) { delay(1); return; }
  if (available()) return;
  // Wake on incoming data instead of sleeping a full tick between USB packets.
  int count = usb_serial_jtag_read_bytes(buffered_, sizeof(buffered_), pdMS_TO_TICKS(1));
  position_ = 0; length_ = count > 0 ? (size_t)count : 0;
}
