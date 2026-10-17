#pragma once
#include <stdint.h>

class WifiAccessPoint {
 public:
  void begin();
  void setEnabled(bool enabled);
  bool enabled() const { return active_; }
  const char *status() const { return status_; }
 private:
  bool healthy() const;
  void retry(const char *reason);
  bool active_ = false;
  bool requested_ = false;
  bool starting_ = false;
  uint32_t nextAttempt_ = 0;
  uint32_t startedAt_ = 0, checkedAt_ = 0;
  const char *status_ = "OFF";
};
