#include "WifiAccessPoint.h"
#include "Config.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <string.h>

void WifiAccessPoint::begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
}

bool WifiAccessPoint::healthy() const {
  if (!WiFi.AP.started() || WiFi.getMode() != WIFI_AP ||
      WiFi.softAPIP() != IPAddress(192, 168, 4, 1) ||
      WiFi.getTxPower() != WIFI_TX_POWER_QDBM) return false;
  wifi_config_t config = {};
  esp_netif_dhcp_status_t dhcp;
  return esp_wifi_get_config(WIFI_IF_AP, &config) == ESP_OK &&
         config.ap.ssid_len == sizeof(WIFI_SSID) - 1 &&
         !memcmp(config.ap.ssid, WIFI_SSID, sizeof(WIFI_SSID) - 1) &&
         !config.ap.ssid_hidden && config.ap.channel == WIFI_CHANNEL &&
         esp_netif_dhcps_get_status(WiFi.AP.netif(), &dhcp) == ESP_OK && dhcp == ESP_NETIF_DHCP_STARTED;
}

void WifiAccessPoint::retry(const char *reason) {
  active_ = starting_ = false;
  status_ = reason;
  // Full driver shutdown, without changing the AP configuration while it is running.
  WiFi.mode(WIFI_OFF);
  nextAttempt_ = millis() + 2000;
}

void WifiAccessPoint::setEnabled(bool enabled) {
  uint32_t now = millis();
  if (!enabled) {
    if (requested_) WiFi.mode(WIFI_OFF);
    requested_ = active_ = starting_ = false;
    status_ = "OFF";
    return;
  }
  if (!requested_) {
    requested_ = true;
    retry("STARTING");
    nextAttempt_ = millis() + 200;
    return;
  }
  if (starting_) {
    // Configuration can generate asynchronous stop/start events. Let them settle.
    if (now - startedAt_ < 300) return;
    if (healthy()) {
      starting_ = false; active_ = true; checkedAt_ = now; status_ = "AP READY";
    }
    else if (now - startedAt_ >= 2000) retry("RETRY: AP NOT READY");
    return;
  }
  if (active_) {
    if (now - checkedAt_ >= 1000) {
      checkedAt_ = now;
      if (!healthy()) retry("RETRY: AP LOST");
    }
    return;
  }
  if ((int32_t)(now - nextAttempt_) < 0) return;
  status_ = "STARTING";
  if (!WiFi.AP.begin()) { retry("ERROR: AP START"); return; }
  if (!WiFi.setTxPower((wifi_power_t)WIFI_TX_POWER_QDBM)) { retry("ERROR: TX POWER"); return; }
  if (!WiFi.softAPbandwidth(WIFI_BW_HT20)) { retry("ERROR: BANDWIDTH"); return; }
  if (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, false, WIFI_MAX_CLIENTS)) {
    retry("ERROR: AP CONFIG"); return;
  }
  const IPAddress address(192, 168, 4, 1), mask(255, 255, 255, 0), firstLease(192, 168, 4, 2);
  if (!WiFi.softAPConfig(address, address, mask, firstLease)) { retry("ERROR: DHCP"); return; }
  starting_ = true; startedAt_ = millis();
}
