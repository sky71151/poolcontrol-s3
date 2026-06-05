#include "SerialComm.h"

#include <cstdio>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {

constexpr size_t kMaxMessageLength = 128;

struct SerialMessage {
  char text[kMaxMessageLength];
  bool addNewLine;
};

QueueHandle_t serialQueue = nullptr;
TaskHandle_t serialTaskHandle = nullptr;
bool serialReady = false;
bool useAnsiColors = false;

bool sendMessage(const char *message, bool addNewLine);

const char *levelTag(SerialComm::LogLevel level) {
  switch (level) {
    case SerialComm::LogLevel::Debug:
      return "DEBUG";
    case SerialComm::LogLevel::Info:
      return "INFO";
    case SerialComm::LogLevel::Warn:
      return "WARN";
    case SerialComm::LogLevel::Error:
      return "ERROR";
    default:
      return "INFO";
  }
}

const char *sourceTag(SerialComm::LogSource source) {
  switch (source) {
    case SerialComm::LogSource::System:
      return "SYSTEM";
    case SerialComm::LogSource::SPI:
      return "SPI";
    case SerialComm::LogSource::Firebase:
      return "FIREBASE";
    case SerialComm::LogSource::Modbus:
      return "MODBUS";
    case SerialComm::LogSource::Canbus:
      return "CANBUS";
    case SerialComm::LogSource::Bluetooth:
      return "BLUETOOTH";
    default:
      return "SYSTEM";
  }
}

const char *levelAnsiColor(SerialComm::LogLevel level) {
  switch (level) {
    case SerialComm::LogLevel::Debug:
      return "\x1b[36m"; // cyan
    case SerialComm::LogLevel::Info:
      return "\x1b[32m"; // green
    case SerialComm::LogLevel::Warn:
      return "\x1b[33m"; // yellow
    case SerialComm::LogLevel::Error:
      return "\x1b[31m"; // red
    default:
      return "\x1b[0m";
  }
}

bool sendFormatted(SerialComm::LogLevel level, SerialComm::LogSource source, const char *message) {
  if (message == nullptr) {
    return false;
  }

  const char *taskName = pcTaskGetName(nullptr);
  if (taskName == nullptr) {
    taskName = "main";
  }

  char formatted[kMaxMessageLength] = {0};
  if (useAnsiColors) {
    snprintf(
      formatted,
      sizeof(formatted),
      "[%lu][%s%s\x1b[0m][%s][%s] %s",
      static_cast<unsigned long>(millis()),
      levelAnsiColor(level),
      levelTag(level),
      sourceTag(source),
      taskName,
      message
    );
  } else {
    snprintf(
      formatted,
      sizeof(formatted),
      "[%lu][%s][%s][%s] %s",
      static_cast<unsigned long>(millis()),
      levelTag(level),
      sourceTag(source),
      taskName,
      message
    );
  }

  return sendMessage(formatted, true);
}

void serialTxTask(void *parameter) {
  (void)parameter;

  SerialMessage message{};

  for (;;) {
    if (xQueueReceive(serialQueue, &message, portMAX_DELAY) == pdTRUE) {
      if (message.addNewLine) {
        Serial.println(message.text);
      } else {
        Serial.print(message.text);
      }
    }
  }
}

bool sendMessage(const char *message, bool addNewLine) {
  if (!serialReady || serialQueue == nullptr || message == nullptr) {
    return false;
  }

  SerialMessage queueMessage{};
  strncpy(queueMessage.text, message, kMaxMessageLength - 1);
  queueMessage.text[kMaxMessageLength - 1] = '\0';
  queueMessage.addNewLine = addNewLine;

  return xQueueSend(serialQueue, &queueMessage, 0) == pdTRUE;
}

}

namespace SerialComm {

void begin(uint32_t baudRate, UBaseType_t taskPriority, uint16_t queueLength) {
  if (serialReady) {
    return;
  }

  Serial.begin(baudRate);

  serialQueue = xQueueCreate(queueLength, sizeof(SerialMessage));
  if (serialQueue == nullptr) {
    return;
  }

  BaseType_t created = xTaskCreate(
    serialTxTask,
    "SerialTxTask",
    3072,
    nullptr,
    taskPriority,
    &serialTaskHandle
  );

  serialReady = (created == pdPASS);
}

void setColorsEnabled(bool enabled) {
  useAnsiColors = enabled;
}

bool colorsEnabled() {
  return useAnsiColors;
}

bool println(const char *message) {
  return sendMessage(message, true);
}

bool print(const char *message) {
  return sendMessage(message, false);
}

bool log(LogLevel level, const char *message) {
  return sendFormatted(level, LogSource::System, message);
}

bool log(LogLevel level, LogSource source, const char *message) {
  return sendFormatted(level, source, message);
}

bool debug(const char *message) {
  return log(LogLevel::Debug, LogSource::System, message);
}

bool debug(LogSource source, const char *message) {
  return log(LogLevel::Debug, source, message);
}

bool info(const char *message) {
  return log(LogLevel::Info, LogSource::System, message);
}

bool info(LogSource source, const char *message) {
  return log(LogLevel::Info, source, message);
}

bool warn(const char *message) {
  return log(LogLevel::Warn, LogSource::System, message);
}

bool warn(LogSource source, const char *message) {
  return log(LogLevel::Warn, source, message);
}

bool error(const char *message) {
  return log(LogLevel::Error, LogSource::System, message);
}

bool error(LogSource source, const char *message) {
  return log(LogLevel::Error, source, message);
}

}
