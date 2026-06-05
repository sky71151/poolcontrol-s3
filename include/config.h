#ifndef CONFIG_H
#define CONFIG_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LedWifi 16
#define LedBluetooth 9
#define Ledheartbeat 46
#define LedFireBase 3

//MCP23S17
#define MCP23S17_CS 8
#define MCP23S17_ADDRESS 0x20
#define MCP23S17_INT 45

//W25Q128JVS
#define W25Q128JVS_CS 10

//SPI pins
#define CLK 12
#define MOSI 11
#define MISO 13

// Firebase
#define FIREBASE_ENABLED 0
#define FIREBASE_API_KEY ""
#define FIREBASE_DATABASE_URL ""
#define FIREBASE_USER_EMAIL ""
#define FIREBASE_USER_PASSWORD ""

// WiFi manager
#define WIFI_MANAGER_ENABLED 1
#define WIFI_MANAGER_AP_NAME "PoolControl-Setup"
#define WIFI_MANAGER_AP_PASSWORD ""
#define WIFI_MANAGER_CONNECT_TIMEOUT_S 180

// GitHub OTA
#define OTA_GITHUB_ENABLED 0
#define OTA_GITHUB_CURRENT_VERSION "1.0.2"
#define OTA_GITHUB_VERSION_URL ""
#define OTA_GITHUB_FIRMWARE_URL ""
#define OTA_GITHUB_CHECK_ON_BOOT 1
#define OTA_GITHUB_CHECK_INTERVAL_S 3600
#define OTA_GITHUB_HTTP_TIMEOUT_MS 15000
#define OTA_GITHUB_ALLOW_INSECURE_TLS 1

// Bluetooth
#define BLUETOOTH_ENABLED 1
#define BLUETOOTH_DEVICE_NAME "PoolControl-S3"

void configurePins();

#endif
