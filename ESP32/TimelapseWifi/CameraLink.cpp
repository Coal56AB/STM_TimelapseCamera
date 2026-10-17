#include "CameraLink.h"
#include "Config.h"
#include <stdio.h>
#include <string.h>

void CameraLink::begin() {
  usb_.begin();
}

void CameraLink::handleCommand() {
  unsigned state;
  unsigned long id;
  char extra;
  unsigned status, codec = 0; unsigned long token, count; unsigned long long size;
  bool fileHeader = sscanf(line_, "FILE %lu %u %lu %llu %lu %u %c", &id, &status, &token, &size, &count, &codec, &extra) == 6;
  if (!fileHeader) { codec = 0; fileHeader = sscanf(line_, "FILE %lu %u %lu %llu %lu %c", &id, &status, &token, &size, &count, &extra) == 5; }
  if (fileHeader && count <= 16384 && codec <= 2) {
    remaining_ = count; received_ = 0;
    copyPayload_ = pending_ && id == pending_ && count <= capacity_ && (payload_ || !count);
    if (copyPayload_) {
      lastReply_ = millis();
      reply_.token = (uint32_t)token; reply_.size = size; reply_.status = status; reply_.length = count;
      reply_.codec = codec;
      fileDone_ = !count;
    }
    return;
  }
  if (sscanf(line_, "WIFI %u %c", &state, &extra) == 1 && state <= 1) {
    requested_ = state != 0;
    lastReply_ = millis();
  }
  else if (sscanf(line_, "OK %lu %c", &id, &extra) == 1 && pending_ && id == pending_)
    confirmed_ = true;
  else if (sscanf(line_, "ERR %lu %c", &id, &extra) == 1 && pending_ && id == pending_)
    rejected_ = true;
}

void CameraLink::update(bool wifiEnabled, const char *wifiStatus) {
  actual_ = wifiEnabled;
  if (wifiStatus) wifiStatus_ = wifiStatus;
  while (usb_.available()) {
    if (remaining_) {
      size_t count = (size_t)usb_.available();
      if (count > remaining_) count = remaining_;
      uint8_t discard[64];
      if (!copyPayload_ && count > sizeof(discard)) count = sizeof(discard);
      count = usb_.read(copyPayload_ ? payload_ + received_ : discard, count);
      if (!count || count > remaining_) break;
      received_ += count; remaining_ -= count;
      if (!remaining_ && copyPayload_) fileDone_ = true;
      continue;
    }
    char byte = (char)usb_.read();
    if (byte == '\n') {
      if (!discard_ && length_) { line_[length_] = 0; handleCommand(); }
      length_ = 0; discard_ = false;
    }
    else if (byte != '\r' && !discard_) {
      if (length_ < sizeof(line_) - 1) line_[length_++] = byte;
      else { length_ = 0; discard_ = true; }
    }
  }
  uint32_t now = millis();
  if (now - lastReply_ >= (fileWaiting_ ? 10000U : LINK_TIMEOUT_MS)) {
    requested_ = false;
    // A disconnected transfer must not consume new control replies as old file bytes.
    remaining_ = length_ = 0; discard_ = copyPayload_ = false;
  }
  if ((int32_t)(now - nextState_) >= 0) {
    const char *state = actual_ ? "STATE 1\n" : "STATE 0\n";
    usb_.write((const uint8_t *)state, strlen(state));
    if (!fileWaiting_) {
      char info[80];
      int length = snprintf(info, sizeof(info), "APSSID %s\n", WIFI_SSID);
      usb_.write((const uint8_t *)info, length);
      length = snprintf(info, sizeof(info), "APPASS %s\n", WIFI_PASSWORD);
      usb_.write((const uint8_t *)info, length);
      const char *status = actual_ && *transferStatus_ && now - transferSeen_ < 3000 ? transferStatus_ : wifiStatus_;
      length = snprintf(info, sizeof(info), "APINFO %.26s\n", status);
      usb_.write((const uint8_t *)info, length);
    }
    nextState_ = now + LINK_STATE_INTERVAL_MS;
  }
}

void CameraLink::reportTransfer(const char *status) {
  snprintf(transferStatus_, sizeof(transferStatus_), "%.26s", status);
  transferSeen_ = millis();
  char info[40];
  int size = snprintf(info, sizeof(info), "APINFO %s\n", transferStatus_);
  usb_.write((const uint8_t *)info, size);
}

bool CameraLink::setTime(uint32_t utc, int16_t offsetMinutes) {
  if (!requested_ || pending_ || utc < 946684800UL || utc > 4102444799UL ||
      offsetMinutes < -720 || offsetMinutes > 840) return false;
  if (++sequence_ == 0) ++sequence_;
  pending_ = sequence_; confirmed_ = rejected_ = false;
  char request[64];
  int count = snprintf(request, sizeof(request), "TIME %lu %lu %d\n",
                       (unsigned long)pending_, (unsigned long)utc, offsetMinutes);
  if (usb_.write((const uint8_t *)request, count) != (size_t)count) {
    pending_ = 0;
    return false;
  }
  uint32_t started = millis();
  while (!confirmed_ && !rejected_ && requested_ && millis() - started < LINK_TIMEOUT_MS) {
    update(actual_);
    delay(1);
  }
  pending_ = 0;
  return confirmed_;
}

bool CameraLink::fileCommand(const char *operation, uint8_t *data, size_t capacity, FileReply &reply) {
  if (!requested_ || pending_) return false;
  if (++sequence_ == 0) ++sequence_;
  pending_ = sequence_; fileDone_ = false; fileWaiting_ = true;
  payload_ = data; capacity_ = capacity;
  char request[96];
  int count = snprintf(request, sizeof(request), "FILES %lu %s\n", (unsigned long)pending_, operation);
  bool sent = count > 0 && (size_t)count < sizeof(request) &&
              usb_.write((const uint8_t *)request, count) == (size_t)count;
  uint32_t started = millis();
  while (sent && !fileDone_ && requested_ && millis() - started < 10000) {
    update(actual_); if (!fileDone_) usb_.wait();
  }
  pending_ = 0; payload_ = nullptr; copyPayload_ = false; fileWaiting_ = false;
  if (!fileDone_) {
    requested_ = false; remaining_ = length_ = 0; discard_ = false;
    return false;
  }
  reply = reply_;
  return reply.status == 0;
}
