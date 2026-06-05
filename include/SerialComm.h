#pragma once

#include <Arduino.h>

namespace SerialComm {

enum class LogLevel : uint8_t {
	Debug,
	Info,
	Warn,
	Error
};

enum class LogSource : uint8_t {
	System,
	SPI,
	Firebase,
	Modbus,
	Canbus,
	Bluetooth
};

void begin(uint32_t baudRate = 115200, UBaseType_t taskPriority = 1, uint16_t queueLength = 16);
void setColorsEnabled(bool enabled);
bool colorsEnabled();
bool println(const char *message);
bool print(const char *message);
bool log(LogLevel level, const char *message);
bool log(LogLevel level, LogSource source, const char *message);
bool debug(const char *message);
bool debug(LogSource source, const char *message);
bool info(const char *message);
bool info(LogSource source, const char *message);
bool warn(const char *message);
bool warn(LogSource source, const char *message);
bool error(const char *message);
bool error(LogSource source, const char *message);

}
