#pragma once

#include <Arduino.h>

namespace FirebaseRTDB {

using StreamCallback = void (*)(
  const char *streamPath,
  const char *dataPath,
  const char *eventType,
  const char *dataType,
  const char *data
);

struct Config {
  bool enabled = false;
  const char *apiKey = nullptr;
  const char *databaseUrl = nullptr;
  const char *userEmail = nullptr;
  const char *userPassword = nullptr;
  uint16_t queueLength = 16;
  UBaseType_t taskPriority = 1;
  uint16_t taskStackWords = 4096;
};

void begin(const Config &config = Config{});
bool isReady();
void setStreamCallback(StreamCallback callback);
bool startStream(const char *path);

bool writeBool(const char *path, bool value);
bool writeInt(const char *path, int32_t value);
bool writeFloat(const char *path, float value, uint8_t decimals = 2);
bool writeString(const char *path, const char *value);

}
