#pragma once

class WifiAccessPoint {
 public:
  void begin();
  void setEnabled(bool enabled);
  bool enabled() const { return active_; }
 private:
  bool active_ = false;
};
