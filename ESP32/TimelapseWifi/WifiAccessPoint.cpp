#include "WifiAccessPoint.h"
#include "Config.h"
#include <WiFi.h>

void WifiAccessPoint::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
}

void WifiAccessPoint::setEnabled(bool enabled) {
  if (enabled == active_) return;
  if (enabled) {
    active_ = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, false, WIFI_MAX_CLIENTS);
    if (!active_) WiFi.mode(WIFI_OFF);
  }
  else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    active_ = false;
  }
}
