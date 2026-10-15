#pragma once
#include <Arduino.h>

#if !CONFIG_IDF_TARGET_ESP32S3
#error Select ESP32S3 Dev Module
#endif
#if !ARDUINO_USB_CDC_ON_BOOT || !ARDUINO_USB_MODE
#error Enable USB CDC On Boot and select Hardware CDC and JTAG
#endif

constexpr char WIFI_SSID[] = "TimelapseCamera";
constexpr char WIFI_PASSWORD[] = "12345678";
constexpr uint8_t WIFI_CHANNEL = 1;
constexpr uint8_t WIFI_MAX_CLIENTS = 4;
constexpr uint32_t LINK_STATE_INTERVAL_MS = 1000;
constexpr uint32_t LINK_TIMEOUT_MS = 5000;
