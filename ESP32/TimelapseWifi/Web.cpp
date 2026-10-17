#include "Web.h"
#include "WebPage.h"
#include <stdlib.h>
#include <stdio.h>

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
  server_.on("/api/recordings", HTTP_GET, [this]() { recordings(); });
  server_.on("/api/fragments", HTTP_GET, [this]() { fragments(); });
  server_.on("/download", HTTP_GET, [this]() { download(); });
}

void Web::recordings() {
  long long after = 0;
  if ((server_.hasArg("after") && !parseNumber(server_.arg("after"), &after)) || after < 0 || after > 9999999) {
    server_.send(400, "text/plain", "Invalid session"); return;
  }
  char command[48]; snprintf(command, sizeof(command), "LIST %lu", (unsigned long)after);
  FileReply reply;
  if (!link_.fileCommand(command, block_, sizeof(block_) - 1, reply)) {
    server_.send(503, "text/plain", "Camera or SD unavailable"); return;
  }
  block_[reply.length] = 0;
  server_.send(200, "application/json", (const char *)block_);
}

void Web::fragments() {
  long long session, after = 0;
  if (!parseNumber(server_.arg("session"), &session) || session < 1 || session > 9999999 ||
      (server_.hasArg("after") && !parseNumber(server_.arg("after"), &after)) || after < 0 || after > 9999999) {
    server_.send(400, "text/plain", "Invalid fragment"); return;
  }
  char command[48]; snprintf(command, sizeof(command), "PARTS %lu %lu", (unsigned long)session, (unsigned long)after);
  FileReply reply;
  if (!link_.fileCommand(command, block_, sizeof(block_) - 1, reply)) {
    server_.send(503, "text/plain", "Camera or SD unavailable"); return;
  }
  block_[reply.length] = 0;
  server_.send(200, "application/json", (const char *)block_);
}

bool Web::writeClient(const uint8_t *data, size_t length) {
  uint32_t progress = millis();
  while (length && server_.client().connected() && link_.wifiRequested()) {
    size_t sent = server_.client().write(data, length);
    if (sent > length) return false;
    if (sent) { data += sent; length -= sent; progress = millis(); }
    else if (millis() - progress >= 5000) return false;
    link_.update(true);
    if (!sent) delay(1);
  }
  return !length;
}

void Web::download() {
  long long session, part;
  if (!parseNumber(server_.arg("session"), &session) || session < 1 || session > 9999999 ||
      !parseNumber(server_.arg("part"), &part) || part < 0 || part > 9999999) {
    server_.send(400, "text/plain", "Invalid recording"); return;
  }
  char command[80], filename[48];
  snprintf(command, sizeof(command), "OPEN %lu %lu", (unsigned long)session, (unsigned long)part);
  FileReply reply;
  if (!link_.fileCommand(command, block_, sizeof(block_), reply) || !reply.size) {
    server_.send(503, "text/plain", "Cannot open recording. Check SD and recording fragments."); return;
  }
  uint32_t token = reply.token;
  uint64_t total = reply.size, position = 0;
  unsigned codec = reply.codec;
  const char *extension = codec == 2 ? "mp4" : "avi";
  if (part) snprintf(filename, sizeof(filename), "R%07lu_F%07lu.%s", (unsigned long)session, (unsigned long)part, extension);
  else snprintf(filename, sizeof(filename), "R%07lu_ALL.%s", (unsigned long)session, extension);
  // Content-Length is 64-bit; WebServer's size_t length is only 32-bit on ESP32.
  char header[320];
  int length = snprintf(header, sizeof(header),
    "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %llu\r\n"
    "Content-Disposition: attachment; filename=\"%s\"\r\nAccept-Ranges: none\r\nConnection: close\r\n\r\n",
    codec == 2 ? "video/mp4" : "video/x-msvideo", (unsigned long long)total, filename);
  server_.client().setTimeout(2);
  server_.client().setNoDelay(true);
  bool ok = writeClient((const uint8_t *)header, (size_t)length);
  uint32_t sampleAt = millis(), usbMs = 0, httpMs = 0, sampleBytes = 0;
  while (ok && position < total && server_.client().connected() && link_.wifiRequested()) {
    snprintf(command, sizeof(command), "READ %lu %llu", (unsigned long)token, (unsigned long long)position);
    uint32_t started = millis();
    ok = link_.fileCommand(command, block_, sizeof(block_), reply) && reply.token == token &&
         reply.size == total && reply.codec == codec && reply.length && reply.length <= total - position;
    usbMs += millis() - started;
    if (ok) {
      started = millis(); ok = writeClient(block_, reply.length); httpMs += millis() - started;
      if (!ok) break;
      position += reply.length; sampleBytes += reply.length;
      if (millis() - sampleAt >= 1000 || position == total) {
        char speed[27];
        unsigned long usb = (uint64_t)sampleBytes * 1000 / (1024ULL * (usbMs ? usbMs : 1));
        unsigned long http = (uint64_t)sampleBytes * 1000 / (1024ULL * (httpMs ? httpMs : 1));
        if (usb > 99999) usb = 99999;
        if (http > 99999) http = 99999;
        snprintf(speed, sizeof(speed), "USB %luK HTTP %luK", usb, http);
        link_.reportTransfer(speed);
        sampleAt = millis(); usbMs = httpMs = sampleBytes = 0;
      }
    }
  }
  snprintf(command, sizeof(command), "CLOSE %lu", (unsigned long)token);
  link_.fileCommand(command, nullptr, 0, reply);
  // A short body remains a failed download; never report truncated data as a complete file.
  server_.client().stop();
}

void Web::update(bool enabled) {
  if (enabled != running_) {
    if (enabled) server_.begin(); else server_.stop();
    running_ = enabled;
  }
  if (running_) server_.handleClient();
}
