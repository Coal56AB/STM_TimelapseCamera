#pragma once
#include "Arduino.h"
#include <cassert>
enum { WIFI_OFF, WIFI_AP };
struct IPAddress {
  uint8_t bytes[4];
  IPAddress(int a, int b, int c, int d): bytes{(uint8_t)a,(uint8_t)b,(uint8_t)c,(uint8_t)d} {}
};
struct TestWiFi {
  int modeValue = WIFI_OFF, attempts = 0, dhcp = 0;
  bool fail = false;
  void persistent(bool) {}
  bool mode(int value) { modeValue = value; return true; }
  bool softAPConfig(IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress first) {
    assert(modeValue == WIFI_AP && ip.bytes[3] == 1 && gateway.bytes[3] == 1 && mask.bytes[3] == 0 && first.bytes[3] == 2);
    ++dhcp; return true;
  }
  bool softAP(const char *, const char *, int, bool, int) { ++attempts; return !fail; }
  void softAPdisconnect(bool) { modeValue = WIFI_OFF; }
};
extern TestWiFi WiFi;
