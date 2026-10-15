#include "TimeLink.h"
#include "Config.h"
#include <stdio.h>
#include <string.h>

void TimeLink::begin() {
  Serial.setRxBufferSize(512);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(50);
}

void TimeLink::handleCommand() {
  unsigned state;
  unsigned long id;
  char extra;
  if (sscanf(line_, "WIFI %u %c", &state, &extra) == 1 && state <= 1) {
    requested_ = state != 0;
    lastReply_ = millis();
  }
  else if (sscanf(line_, "OK %lu %c", &id, &extra) == 1 && pending_ && id == pending_)
    confirmed_ = true;
  else if (sscanf(line_, "ERR %lu %c", &id, &extra) == 1 && pending_ && id == pending_)
    rejected_ = true;
}

void TimeLink::update(bool wifiEnabled) {
  actual_ = wifiEnabled;
  while (Serial.available()) {
    char byte = (char)Serial.read();
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
  if (now - lastReply_ >= LINK_TIMEOUT_MS) requested_ = false;
  if ((int32_t)(now - nextState_) >= 0) {
    const char *state = actual_ ? "STATE 1\n" : "STATE 0\n";
    Serial.write((const uint8_t *)state, strlen(state));
    nextState_ = now + LINK_STATE_INTERVAL_MS;
  }
}

bool TimeLink::setTime(uint32_t utc, int16_t offsetMinutes) {
  if (!requested_ || pending_ || utc < 946684800UL || utc > 4102444799UL ||
      offsetMinutes < -720 || offsetMinutes > 840) return false;
  if (++sequence_ == 0) ++sequence_;
  pending_ = sequence_; confirmed_ = rejected_ = false;
  char request[64];
  int count = snprintf(request, sizeof(request), "TIME %lu %lu %d\n",
                       (unsigned long)pending_, (unsigned long)utc, offsetMinutes);
  if (Serial.write((const uint8_t *)request, count) != (size_t)count) {
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
