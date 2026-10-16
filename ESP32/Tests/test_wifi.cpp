#include "WiFi.h"
#include "WifiAccessPoint.h"
#include <cstdio>
TestWiFi WiFi;
static uint32_t tick;
uint32_t millis() { return tick; }
int main() {
  WifiAccessPoint ap; ap.begin();
  WiFi.fail = true; ap.setEnabled(true); assert(!ap.enabled() && WiFi.attempts == 1);
  for (tick = 1; tick < 2000; ++tick) ap.setEnabled(true);
  assert(WiFi.attempts == 1);
  WiFi.fail = false; ap.setEnabled(true); assert(ap.enabled() && WiFi.attempts == 2 && WiFi.dhcp == 2);
  ap.setEnabled(true); assert(WiFi.attempts == 2);
  ap.setEnabled(false); assert(!ap.enabled() && WiFi.modeValue == WIFI_OFF);
  ap.setEnabled(true); assert(ap.enabled() && WiFi.attempts == 3);
  ap.setEnabled(false); tick = UINT32_MAX - 500; WiFi.fail = true; ap.setEnabled(true);
  tick += 1999; ap.setEnabled(true); assert(WiFi.attempts == 4);
  tick += 1; WiFi.fail = false; ap.setEnabled(true); assert(ap.enabled() && WiFi.attempts == 5);
  puts("PASS: AP/DHCP setup, bounded retry after failure, disable/re-enable and tick wrap");
}
