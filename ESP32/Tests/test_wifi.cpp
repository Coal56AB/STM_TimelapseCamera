#include "WiFi.h"
#include "WifiAccessPoint.h"
#include <cstdio>
TestWiFi WiFi;
static uint32_t tick;
uint32_t millis() { return tick; }
int main() {
  WifiAccessPoint ap; ap.begin();
  WiFi.fail = true; ap.setEnabled(true); assert(!ap.enabled() && WiFi.attempts == 0);
  tick += 200; ap.setEnabled(true); assert(WiFi.attempts == 1);
  for (tick = 201; tick < 2200; ++tick) ap.setEnabled(true);
  assert(WiFi.attempts == 1);
  WiFi.fail = false; ap.setEnabled(true); assert(!ap.enabled() && WiFi.attempts == 2);
  tick += 300; ap.setEnabled(true); assert(ap.enabled() && WiFi.power == 34 && WiFi.dhcpRunning);
  ap.setEnabled(true); assert(WiFi.attempts == 2);
  for (unsigned i = 0; i < 120; ++i) { tick += 1000; ap.setEnabled(true); }
  assert(ap.enabled() && WiFi.attempts == 2); // No clients is not a fault.
  WiFi.AP.up = false; tick += 1000; ap.setEnabled(true);
  assert(!ap.enabled() && !strcmp(ap.status(), "RETRY: AP LOST"));
  tick += 2000; ap.setEnabled(true); tick += 300; ap.setEnabled(true); assert(ap.enabled());
  WiFi.dhcpRunning = false; tick += 1000; ap.setEnabled(true); assert(!ap.enabled());
  tick += 2000; ap.setEnabled(true);
  WiFi.hidden = true; tick += 2000; ap.setEnabled(true);
  assert(!ap.enabled() && !strcmp(ap.status(), "RETRY: AP NOT READY"));
  ap.setEnabled(false); assert(!ap.enabled() && WiFi.modeValue == WIFI_OFF);
  tick = UINT32_MAX - 100; ap.setEnabled(true);
  tick += 200; ap.setEnabled(true); tick += 300; ap.setEnabled(true); assert(ap.enabled());
  bool *failures[] = {&WiFi.startFail, &WiFi.powerFail, &WiFi.bandwidthFail, &WiFi.fail, &WiFi.dhcpFail};
  const char *errors[] = {"ERROR: AP START", "ERROR: TX POWER", "ERROR: BANDWIDTH", "ERROR: AP CONFIG", "ERROR: DHCP"};
  for (unsigned i = 0; i < 5; ++i) {
    ap.setEnabled(false); *failures[i] = true; ap.setEnabled(true); tick += 200; ap.setEnabled(true);
    assert(!ap.enabled() && !strcmp(ap.status(), errors[i]));
    *failures[i] = false; tick += 2000; ap.setEnabled(true); tick += 300; ap.setEnabled(true); assert(ap.enabled());
  }
  puts("PASS: AP startup verification, radio power, DHCP/SSID health, lost AP recovery, failed stages, stable idle and tick wrap");
}
