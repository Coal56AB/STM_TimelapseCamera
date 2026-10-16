#pragma once
#include <stdint.h>

class WifiAccessPoint {
 public:
  void begin();
  void setEnabled(bool enabled);
  bool enabled() const { return active_; }
 private:
  bool active_ = false;
  bool requested_ = false;
  uint32_t nextAttempt_ = 0;
};
