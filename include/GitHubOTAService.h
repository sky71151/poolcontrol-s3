#pragma once

#include <Arduino.h>

namespace GitHubOTAService {

struct Config {
  bool enabled = false;
  const char *versionUrl = nullptr;
  const char *firmwareUrl = nullptr;
  const char *currentVersion = "0.0.0";
  uint32_t checkIntervalSeconds = 3600;
  uint32_t httpTimeoutMs = 15000;
  bool checkOnBoot = true;
  bool allowInsecureTls = true;
  UBaseType_t taskPriority = 1;
  uint16_t taskStackWords = 8192;
};

void begin(const Config &config = Config{});
bool isReady();
void requestCheckNow();

}
