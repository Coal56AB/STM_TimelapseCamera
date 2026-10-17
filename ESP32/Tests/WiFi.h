#pragma once
#include "Arduino.h"
#include <cassert>
enum { WIFI_OFF, WIFI_AP, WIFI_BW_HT20 };
typedef int wifi_power_t;
struct IPAddress {
  uint8_t bytes[4];
  IPAddress(int a, int b, int c, int d): bytes{(uint8_t)a,(uint8_t)b,(uint8_t)c,(uint8_t)d} {}
  bool operator!=(const IPAddress &other) const { return memcmp(bytes, other.bytes, 4) != 0; }
};
struct TestAP {
  bool up = false;
  bool begin();
  bool started() const { return up; }
  void *netif() { return this; }
};
struct TestWiFi {
  TestAP AP;
  int modeValue = WIFI_OFF, attempts = 0, dhcp = 0;
  int power = 78;
  bool fail = false, startFail = false, powerFail = false, dhcpFail = false, bandwidthFail = false, configReadFail = false;
  bool dhcpRunning = false, hidden = false;
  std::string ssid;
  int channel = 1;
  void persistent(bool) {}
  bool mode(int value) { modeValue = value; if (value == WIFI_OFF) { AP.up = false; dhcpRunning = false; } return true; }
  int getMode() { return modeValue; }
  bool setTxPower(wifi_power_t value) { if (powerFail) return false; power = value; return true; }
  int getTxPower() { return power; }
  bool softAPbandwidth(int value) { assert(value == WIFI_BW_HT20); return !bandwidthFail; }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  bool softAPConfig(IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress first) {
    assert(modeValue == WIFI_AP && ip.bytes[3] == 1 && gateway.bytes[3] == 1 && mask.bytes[3] == 0 && first.bytes[3] == 2);
    ++dhcp; dhcpRunning = !dhcpFail; return !dhcpFail;
  }
  bool softAP(const char *name, const char *, int chan, bool hide, int) { ++attempts; ssid = name; channel = chan; hidden = hide; return !fail; }
};
extern TestWiFi WiFi;
inline bool TestAP::begin() { if (WiFi.startFail) return false; WiFi.modeValue = WIFI_AP; up = true; return true; }
