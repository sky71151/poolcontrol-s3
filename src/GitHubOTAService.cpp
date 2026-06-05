#include "GitHubOTAService.h"

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "SerialComm.h"

namespace {

GitHubOTAService::Config runtimeConfig{};
TaskHandle_t otaTaskHandle = nullptr;
bool otaReady = false;
volatile bool forcedCheckRequested = false;

bool isNonEmpty(const char *text) {
  return text != nullptr && text[0] != '\0';
}

String sanitizeVersion(const String &raw) {
  String cleaned = raw;
  cleaned.trim();
  cleaned.replace("\r", "");
  cleaned.replace("\n", "");

  if (cleaned.length() >= 2 && cleaned[0] == '"' && cleaned[cleaned.length() - 1] == '"') {
    cleaned = cleaned.substring(1, cleaned.length() - 1);
  }

  cleaned.trim();
  return cleaned;
}

bool fetchLatestVersion(String &latestVersionOut) {
  if (!isNonEmpty(runtimeConfig.versionUrl)) {
    return false;
  }

  WiFiClientSecure client;
  if (runtimeConfig.allowInsecureTls) {
    client.setInsecure();
  }

  HTTPClient http;
  http.setTimeout(runtimeConfig.httpTimeoutMs);
  if (!http.begin(client, runtimeConfig.versionUrl)) {
    SerialComm::error(SerialComm::LogSource::System, "OTA version request init failed");
    return false;
  }

  int responseCode = http.GET();
  if (responseCode != HTTP_CODE_OK) {
    char message[96] = {0};
    snprintf(message, sizeof(message), "OTA version HTTP error: %d", responseCode);
    SerialComm::warn(SerialComm::LogSource::System, message);
    http.end();
    return false;
  }

  latestVersionOut = sanitizeVersion(http.getString());
  http.end();

  if (latestVersionOut.isEmpty()) {
    SerialComm::warn(SerialComm::LogSource::System, "OTA version response was empty");
    return false;
  }

  return true;
}

bool runFirmwareUpdate() {
  if (!isNonEmpty(runtimeConfig.firmwareUrl)) {
    SerialComm::warn(SerialComm::LogSource::System, "OTA firmware URL not configured");
    return false;
  }

  SerialComm::info(SerialComm::LogSource::System, "OTA update started");

  WiFiClientSecure client;
  if (runtimeConfig.allowInsecureTls) {
    client.setInsecure();
  }

  t_httpUpdate_return result;
  if (isNonEmpty(runtimeConfig.currentVersion)) {
    result = httpUpdate.update(client, runtimeConfig.firmwareUrl, runtimeConfig.currentVersion);
  } else {
    result = httpUpdate.update(client, runtimeConfig.firmwareUrl);
  }

  switch (result) {
    case HTTP_UPDATE_FAILED: {
      char message[128] = {0};
      snprintf(
        message,
        sizeof(message),
        "OTA failed: (%d) %s",
        httpUpdate.getLastError(),
        httpUpdate.getLastErrorString().c_str()
      );
      SerialComm::error(SerialComm::LogSource::System, message);
      return false;
    }
    case HTTP_UPDATE_NO_UPDATES:
      SerialComm::info(SerialComm::LogSource::System, "OTA: no update available from server");
      return false;
    case HTTP_UPDATE_OK:
      SerialComm::info(SerialComm::LogSource::System, "OTA successful; rebooting");
      return true;
    default:
      return false;
  }
}

void checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) {
    SerialComm::warn(SerialComm::LogSource::System, "OTA skipped: WiFi not connected");
    return;
  }

  if (isNonEmpty(runtimeConfig.versionUrl)) {
    String latestVersion;
    if (!fetchLatestVersion(latestVersion)) {
      return;
    }

    if (isNonEmpty(runtimeConfig.currentVersion) && latestVersion == runtimeConfig.currentVersion) {
      SerialComm::info(SerialComm::LogSource::System, "OTA check: device already up to date");
      return;
    }

    char message[128] = {0};
    snprintf(
      message,
      sizeof(message),
      "OTA update available: current=%s latest=%s",
      isNonEmpty(runtimeConfig.currentVersion) ? runtimeConfig.currentVersion : "unknown",
      latestVersion.c_str()
    );
    SerialComm::info(SerialComm::LogSource::System, message);
  }

  (void)runFirmwareUpdate();
}

void otaTask(void *parameter) {
  (void)parameter;

  const TickType_t stepDelay = pdMS_TO_TICKS(1000);
  TickType_t secondsCounter = 0;

  if (runtimeConfig.checkOnBoot) {
    checkForUpdates();
  }

  for (;;) {
    if (forcedCheckRequested) {
      forcedCheckRequested = false;
      checkForUpdates();
      secondsCounter = 0;
      vTaskDelay(stepDelay);
      continue;
    }

    if (runtimeConfig.checkIntervalSeconds > 0) {
      if (secondsCounter >= runtimeConfig.checkIntervalSeconds) {
        checkForUpdates();
        secondsCounter = 0;
      } else {
        secondsCounter++;
      }
    }

    vTaskDelay(stepDelay);
  }
}

}  // namespace

namespace GitHubOTAService {

void begin(const Config &config) {
  if (otaReady) {
    return;
  }

  runtimeConfig = config;
  if (!runtimeConfig.enabled) {
    SerialComm::warn(SerialComm::LogSource::System, "GitHub OTA is disabled");
    return;
  }

  if (!isNonEmpty(runtimeConfig.firmwareUrl)) {
    SerialComm::warn(SerialComm::LogSource::System, "GitHub OTA firmware URL is empty");
    return;
  }

  BaseType_t created = xTaskCreate(
    otaTask,
    "GitHubOtaTask",
    runtimeConfig.taskStackWords,
    nullptr,
    runtimeConfig.taskPriority,
    &otaTaskHandle
  );

  otaReady = (created == pdPASS);
  if (otaReady) {
    SerialComm::info(SerialComm::LogSource::System, "GitHub OTA task started");
  } else {
    SerialComm::error(SerialComm::LogSource::System, "GitHub OTA task create failed");
  }
}

bool isReady() {
  return otaReady;
}

void requestCheckNow() {
  forcedCheckRequested = true;
}

}
