#include "WifiAccessPoint.h"
#include "Config.h"
#include <WiFi.h>

void WifiAccessPoint::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
}

void WifiAccessPoint::setEnabled(bool enabled) {
  if (enabled != requested_) { requested_ = enabled; nextAttempt_ = millis(); }
  if (enabled == active_) return;
  if (enabled) {
    if ((int32_t)(millis() - nextAttempt_) < 0) return;
    nextAttempt_ = millis() + 2000;
    const IPAddress address(192, 168, 4, 1), mask(255, 255, 255, 0), firstLease(192, 168, 4, 2);
    active_ = WiFi.mode(WIFI_AP) &&
              WiFi.softAPConfig(address, address, mask, firstLease) &&
              WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, false, WIFI_MAX_CLIENTS);
    if (!active_) WiFi.mode(WIFI_OFF);
  }
  else {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    active_ = false;
  }
}
