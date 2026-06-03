#pragma once

#include <Arduino.h>

#include "../protocol/protocol_errors.h"

class CommandQueue;
class Protocol;
class Runtime;

class Watchdog {
 public:
  void begin();
  void begin(Runtime& runtime, Protocol& protocol, CommandQueue& commandQueue, Stream& stream);
  void update();

 private:
  void recover(ProtocolError error);

  Runtime* runtime_ = nullptr;
  Protocol* protocol_ = nullptr;
  CommandQueue* commandQueue_ = nullptr;
  Stream* stream_ = nullptr;
  uint32_t lastCheckMs_ = 0;
};
