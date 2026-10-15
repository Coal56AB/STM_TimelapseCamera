#pragma once
#include "WifiAccessPoint.h"
#include "CameraLink.h"
#include "Web.h"

class App {
 public:
  void begin();
  void update();
 private:
  WifiAccessPoint network_;
  CameraLink link_;
  Web web_{link_};
};
