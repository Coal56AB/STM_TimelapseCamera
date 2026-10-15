#pragma once
#include <stdint.h>
#include <stddef.h>

struct FileReply {
  uint32_t token = 0;
  uint64_t size = 0;
  size_t length = 0;
  unsigned status = 0;
};

class CameraLink {
 public:
  void begin();
  void update(bool wifiEnabled);
  bool wifiRequested() const { return requested_; }
  bool setTime(uint32_t utc, int16_t offsetMinutes);
  bool fileCommand(const char *operation, uint8_t *data, size_t capacity, FileReply &reply);
 private:
  void handleCommand();
  char line_[96] = {};
  size_t length_ = 0;
  uint32_t nextState_ = 0, lastReply_ = 0, sequence_ = 0, pending_ = 0;
  bool discard_ = false, requested_ = false, actual_ = false;
  bool confirmed_ = false, rejected_ = false;
  FileReply reply_;
  uint8_t *payload_ = nullptr;
  size_t capacity_ = 0, remaining_ = 0, received_ = 0;
  bool fileDone_ = false, copyPayload_ = false;
  bool fileWaiting_ = false;
};
