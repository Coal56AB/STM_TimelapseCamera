#pragma once
#include "CameraLink.h"
#include <WebServer.h>

class Web {
 public:
  explicit Web(CameraLink &link) : link_(link) {}
  void begin();
  void update(bool enabled);
 private:
  void setTime();
  void recordings();
  void fragments();
  void download();
  bool writeClient(const uint8_t *data, size_t length);
  uint8_t block_[4096];
  WebServer server_{80};
  CameraLink &link_;
  bool running_ = false;
};
