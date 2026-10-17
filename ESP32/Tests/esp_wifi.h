#pragma once
#include "WiFi.h"
#include <cstring>
enum { ESP_OK, WIFI_IF_AP };
struct wifi_config_t { struct { uint8_t ssid[32], ssid_len, ssid_hidden, channel; } ap; };
inline int esp_wifi_get_config(int, wifi_config_t *config) {
  if (WiFi.configReadFail) return -1;
  memcpy(config->ap.ssid, WiFi.ssid.data(), WiFi.ssid.size());
  config->ap.ssid_len = WiFi.ssid.size(); config->ap.ssid_hidden = WiFi.hidden; config->ap.channel = WiFi.channel;
  return ESP_OK;
}
