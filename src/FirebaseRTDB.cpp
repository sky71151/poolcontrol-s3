#include "FirebaseRTDB.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "SerialComm.h"

namespace {

constexpr size_t kMaxPathLength = 96;
constexpr size_t kMaxValueLength = 64;

enum class WriteType : uint8_t {
  Bool,
  Int,
  Float,
  String
};

struct WriteRequest {
  WriteType type;
  char path[kMaxPathLength];
  char value[kMaxValueLength];
};

QueueHandle_t firebaseQueue = nullptr;
TaskHandle_t firebaseTaskHandle = nullptr;
bool ready = false;
FirebaseRTDB::Config runtimeConfig{};
FirebaseData firebaseData;
FirebaseData firebaseStreamData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;
bool firebaseClientReady = false;
FirebaseRTDB::StreamCallback streamCallback = nullptr;
bool streamRequested = false;
bool streamStarted = false;
char streamRequestedPath[kMaxPathLength] = {0};

bool isNonEmpty(const char *text) {
  return text != nullptr && text[0] != '\0';
}

bool initFirebaseClient() {
  if (!runtimeConfig.enabled) {
    return false;
  }

  if (!isNonEmpty(runtimeConfig.apiKey) || !isNonEmpty(runtimeConfig.databaseUrl)) {
    SerialComm::error(SerialComm::LogSource::Firebase, "Firebase API key or database URL missing");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    SerialComm::error(SerialComm::LogSource::Firebase, "WiFi not connected");
    return false;
  }

  firebaseConfig.api_key = runtimeConfig.apiKey;
  firebaseConfig.database_url = runtimeConfig.databaseUrl;

  if (isNonEmpty(runtimeConfig.userEmail) && isNonEmpty(runtimeConfig.userPassword)) {
    firebaseAuth.user.email = runtimeConfig.userEmail;
    firebaseAuth.user.password = runtimeConfig.userPassword;
  }

  Firebase.reconnectWiFi(true);
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  firebaseClientReady = true;
  SerialComm::info(SerialComm::LogSource::Firebase, "Firebase client initialized");
  return true;
}

bool ensureStreamStarted() {
  if (!streamRequested || streamStarted) {
    return streamStarted;
  }

  if (!firebaseClientReady && !initFirebaseClient()) {
    return false;
  }

  if (!Firebase.RTDB.beginStream(&firebaseStreamData, streamRequestedPath)) {
    String reason = firebaseStreamData.errorReason();
    SerialComm::error(SerialComm::LogSource::Firebase, reason.c_str());
    return false;
  }

  streamStarted = true;
  SerialComm::info(SerialComm::LogSource::Firebase, "Firebase stream started");
  return true;
}

void pollStream() {
  if (!streamRequested || !streamStarted) {
    return;
  }

  if (!Firebase.RTDB.readStream(&firebaseStreamData)) {
    String reason = firebaseStreamData.errorReason();
    SerialComm::warn(SerialComm::LogSource::Firebase, reason.c_str());
    streamStarted = false;
    return;
  }

  if (!firebaseStreamData.streamAvailable()) {
    return;
  }

  String sPath = firebaseStreamData.streamPath();
  String dPath = firebaseStreamData.dataPath();
  String eType = firebaseStreamData.eventType();
  String dType = firebaseStreamData.dataType();
  String data = firebaseStreamData.stringData();

  if (streamCallback != nullptr) {
    streamCallback(
      sPath.c_str(),
      dPath.c_str(),
      eType.c_str(),
      dType.c_str(),
      data.c_str()
    );
  }
}

bool enqueueWrite(WriteType type, const char *path, const char *value) {
  if (!ready || firebaseQueue == nullptr || path == nullptr || value == nullptr) {
    return false;
  }

  WriteRequest request{};
  request.type = type;
  strncpy(request.path, path, kMaxPathLength - 1);
  request.path[kMaxPathLength - 1] = '\0';
  strncpy(request.value, value, kMaxValueLength - 1);
  request.value[kMaxValueLength - 1] = '\0';

  return xQueueSend(firebaseQueue, &request, 0) == pdTRUE;
}

bool processWrite(const WriteRequest &request) {
  if (!runtimeConfig.enabled) {
    return false;
  }

  if (!firebaseClientReady && !initFirebaseClient()) {
    return false;
  }

  bool ok = false;
  switch (request.type) {
    case WriteType::Bool: {
      bool value = strcmp(request.value, "true") == 0;
      ok = Firebase.RTDB.setBool(&firebaseData, request.path, value);
      break;
    }
    case WriteType::Int: {
      int32_t value = static_cast<int32_t>(strtol(request.value, nullptr, 10));
      ok = Firebase.RTDB.setInt(&firebaseData, request.path, value);
      break;
    }
    case WriteType::Float: {
      float value = strtof(request.value, nullptr);
      ok = Firebase.RTDB.setFloat(&firebaseData, request.path, value);
      break;
    }
    case WriteType::String:
      ok = Firebase.RTDB.setString(&firebaseData, request.path, request.value);
      break;
    default:
      ok = false;
      break;
  }

  if (!ok) {
    String reason = firebaseData.errorReason();
    SerialComm::error(SerialComm::LogSource::Firebase, reason.c_str());
  }

  return ok;
}

void firebaseTask(void *parameter) {
  (void)parameter;
  WriteRequest request{};

  for (;;) {
    (void)ensureStreamStarted();
    pollStream();

    if (xQueueReceive(firebaseQueue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {
      bool ok = processWrite(request);
      if (!ok && !runtimeConfig.enabled) {
        SerialComm::warn(SerialComm::LogSource::Firebase, "Firebase disabled: write skipped");
      }
    }
  }
}

}

namespace FirebaseRTDB {

void begin(const Config &config) {
  if (ready) {
    return;
  }

  runtimeConfig = config;
  firebaseQueue = xQueueCreate(runtimeConfig.queueLength, sizeof(WriteRequest));
  if (firebaseQueue == nullptr) {
    SerialComm::error(SerialComm::LogSource::Firebase, "Firebase queue create failed");
    return;
  }

  BaseType_t created = xTaskCreate(
    firebaseTask,
    "FirebaseTask",
    runtimeConfig.taskStackWords,
    nullptr,
    runtimeConfig.taskPriority,
    &firebaseTaskHandle
  );

  ready = (created == pdPASS);
  if (ready) {
    SerialComm::info(SerialComm::LogSource::Firebase, "FirebaseRTDB task started");
    if (runtimeConfig.enabled) {
      (void)initFirebaseClient();
    }
  } else {
    SerialComm::error(SerialComm::LogSource::Firebase, "Firebase task create failed");
  }
}

bool isReady() {
  return ready;
}

void setStreamCallback(StreamCallback callback) {
  streamCallback = callback;
}

bool startStream(const char *path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }

  strncpy(streamRequestedPath, path, kMaxPathLength - 1);
  streamRequestedPath[kMaxPathLength - 1] = '\0';
  streamRequested = true;
  streamStarted = false;
  return true;
}

bool writeBool(const char *path, bool value) {
  return enqueueWrite(WriteType::Bool, path, value ? "true" : "false");
}

bool writeInt(const char *path, int32_t value) {
  char buffer[kMaxValueLength] = {0};
  snprintf(buffer, sizeof(buffer), "%ld", static_cast<long>(value));
  return enqueueWrite(WriteType::Int, path, buffer);
}

bool writeFloat(const char *path, float value, uint8_t decimals) {
  char format[8] = {0};
  snprintf(format, sizeof(format), "%%.%uf", static_cast<unsigned int>(decimals));

  char buffer[kMaxValueLength] = {0};
  snprintf(buffer, sizeof(buffer), format, static_cast<double>(value));
  return enqueueWrite(WriteType::Float, path, buffer);
}

bool writeString(const char *path, const char *value) {
  return enqueueWrite(WriteType::String, path, value);
}

}
