#include "Web.h"
#include "WebPage.h"
#include <stdlib.h>

static bool parseNumber(const String &text, long long *value) {
  if (!text.length() || text.length() > 12) return false;
  unsigned start = text[0] == '-' ? 1 : 0;
  if (start == text.length()) return false;
  for (unsigned i = start; i < text.length(); ++i)
    if (text[i] < '0' || text[i] > '9') return false;
  *value = strtoll(text.c_str(), nullptr, 10);
  return true;
}

void Web::setTime() {
  long long utc, offset;
  if (!parseNumber(server_.arg("utc"), &utc) || !parseNumber(server_.arg("offset"), &offset) ||
      utc < 946684800LL || utc > 4102444799LL || offset < -720 || offset > 840) {
    server_.send(400, "text/plain", "Invalid time");
    return;
  }
  bool ok = link_.setTime((uint32_t)utc, (int16_t)offset);
  server_.send(ok ? 200 : 503, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void Web::begin() {
  server_.on("/", HTTP_GET, [this]() { server_.send_P(200, "text/html; charset=utf-8", Web_Page); });
  server_.on("/api/time", HTTP_POST, [this]() { setTime(); });
}

void Web::update(bool enabled) {
  if (enabled != running_) {
    if (enabled) server_.begin(); else server_.stop();
    running_ = enabled;
  }
  if (running_) server_.handleClient();
}
