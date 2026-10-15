#pragma once
#include "WifiAccessPoint.h"
#include "TimeLink.h"
#include "Web.h"

class App {
 public:
  void begin();
  void update();
 private:
  WifiAccessPoint network_;
  TimeLink link_;
  Web web_{link_};
};
