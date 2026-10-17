#pragma once
#include <stdint.h>
#include <stddef.h>
#define ESP_OK 0
#define pdMS_TO_TICKS(value) (value)
struct usb_serial_jtag_driver_config_t { uint32_t tx_buffer_size, rx_buffer_size; };
int usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t *config);
int usb_serial_jtag_read_bytes(void *data, uint32_t size, unsigned ticks);
int usb_serial_jtag_write_bytes(const void *data, size_t size, unsigned ticks);
