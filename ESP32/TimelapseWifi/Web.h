#pragma once
#include "TimeLink.h"
#include <WebServer.h>

class Web {
 public:
  explicit Web(TimeLink &link) : link_(link) {}
  void begin();
  void update(bool enabled);
 private:
  void setTime();
  WebServer server_{80};
  TimeLink &link_;
  bool running_ = false;
};
