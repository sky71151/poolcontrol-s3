#pragma once

#include <Arduino.h>

namespace WiFiManagerService {

struct Config {
  bool enabled = true;
  const char *portalApName = "PoolControl-Setup";
  const char *portalApPassword = "";
  uint16_t connectTimeoutSeconds = 120;
  bool autoRestartOnSave = true;
};

void begin(const Config &config = Config{});
bool isReady();
bool isConnected();
String localIp();

}
