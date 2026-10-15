#pragma once
#include <stdint.h>
#include <stddef.h>

class TimeLink {
 public:
  void begin();
  void update(bool wifiEnabled);
  bool wifiRequested() const { return requested_; }
  bool setTime(uint32_t utc, int16_t offsetMinutes);
 private:
  void handleCommand();
  char line_[96] = {};
  size_t length_ = 0;
  uint32_t nextState_ = 0, lastReply_ = 0, sequence_ = 0, pending_ = 0;
  bool discard_ = false, requested_ = false, actual_ = false;
  bool confirmed_ = false, rejected_ = false;
};
