#pragma once

#include <Arduino.h>

namespace BluetoothComm {

using ReceiveCallback = void (*)(const char *message);

struct Config {
  bool enabled = false;
  const char *deviceName = "PoolControl";
  uint16_t queueLength = 16;
  UBaseType_t taskPriority = 1;
  uint16_t taskStackWords = 3072;
};

void begin(const Config &config = Config{});
bool isReady();

bool sendLine(const char *message);
void setReceiveCallback(ReceiveCallback callback);

}
