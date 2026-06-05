#include "BluetoothComm.h"

#include <cstdio>
#include <cstring>
#include <string>
#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#if __has_include(<soc/soc_caps.h>)
#include <soc/soc_caps.h>
#endif

#include "SerialComm.h"

#if __has_include(<BLEDevice.h>)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define BLUETOOTH_COMM_HAS_BLE 1
#else
#define BLUETOOTH_COMM_HAS_BLE 0
#endif

#if __has_include(<BluetoothSerial.h>) && defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED) && defined(CONFIG_BT_SPP_ENABLED) && defined(SOC_BT_CLASSIC_SUPPORTED) && SOC_BT_CLASSIC_SUPPORTED
#include <BluetoothSerial.h>
#define BLUETOOTH_COMM_HAS_CLASSIC 1
#else
#define BLUETOOTH_COMM_HAS_CLASSIC 0
#endif

namespace {

constexpr size_t kMaxMessageLength = 128;
constexpr char kServiceUuid[] = "7f6c9101-5f9d-4f1e-9fbe-82be95c8a111";
constexpr char kTxUuid[] = "7f6c9102-5f9d-4f1e-9fbe-82be95c8a111";
constexpr char kRxUuid[] = "7f6c9103-5f9d-4f1e-9fbe-82be95c8a111";

struct OutgoingMessage {
  char text[kMaxMessageLength];
};

QueueHandle_t txQueue = nullptr;
TaskHandle_t bluetoothTaskHandle = nullptr;
bool ready = false;
BluetoothComm::Config runtimeConfig{};
BluetoothComm::ReceiveCallback receiveCallback = nullptr;
bool transportAvailable = false;

#if BLUETOOTH_COMM_HAS_CLASSIC
BluetoothSerial serialBt;
#endif

#if BLUETOOTH_COMM_HAS_BLE
BLEServer *bleServer = nullptr;
BLECharacteristic *txCharacteristic = nullptr;
bool bleClientConnected = false;

class BleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    (void)server;
    digitalWrite(LedBluetooth, HIGH);
    bleClientConnected = true;
    SerialComm::info(SerialComm::LogSource::Bluetooth, "BLE client connected");
  }

  void onDisconnect(BLEServer *server) override {
    bleClientConnected = false;
    digitalWrite(LedBluetooth, LOW);
    server->getAdvertising()->start();
    SerialComm::info(SerialComm::LogSource::Bluetooth, "BLE client disconnected, advertising restarted");
  }
};

class BleRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    if (receiveCallback == nullptr) {
      return;
    }

    auto payload = characteristic->getValue();
    if (payload.length() > 0) {
      receiveCallback(payload.c_str());
    }
  }
};
#endif

bool enqueueLine(const char *message) {
  if (!ready || txQueue == nullptr || message == nullptr) {
    return false;
  }

  OutgoingMessage queued{};
  strncpy(queued.text, message, kMaxMessageLength - 1);
  queued.text[kMaxMessageLength - 1] = '\0';

  return xQueueSend(txQueue, &queued, 0) == pdTRUE;
}

void bluetoothTask(void *parameter) {
  (void)parameter;
  OutgoingMessage outgoing{};

  for (;;) {
#if BLUETOOTH_COMM_HAS_CLASSIC
    if (serialBt.available() > 0) {
      String line = serialBt.readStringUntil('\n');
      line.trim();
      if (line.length() > 0 && receiveCallback != nullptr) {
        receiveCallback(line.c_str());
      }
    }
#endif

    if (xQueueReceive(txQueue, &outgoing, pdMS_TO_TICKS(20)) == pdTRUE) {
#if BLUETOOTH_COMM_HAS_BLE
  if (txCharacteristic != nullptr && bleClientConnected) {
    txCharacteristic->setValue(reinterpret_cast<uint8_t *>(outgoing.text), strlen(outgoing.text));
    txCharacteristic->notify();
    continue;
  }
#endif
#if BLUETOOTH_COMM_HAS_CLASSIC
      serialBt.println(outgoing.text);
#else
      (void)outgoing;
  if (!transportAvailable) {
    SerialComm::warn(SerialComm::LogSource::Bluetooth, "Bluetooth transport unavailable on this target");
  }
#endif
    }
  }
}

}

namespace BluetoothComm {

void begin(const Config &config) {
  if (ready) {
    return;
  }

  runtimeConfig = config;
  if (!runtimeConfig.enabled) {
    SerialComm::info(SerialComm::LogSource::Bluetooth, "Bluetooth module disabled");
    return;
  }

  txQueue = xQueueCreate(runtimeConfig.queueLength, sizeof(OutgoingMessage));
  if (txQueue == nullptr) {
    SerialComm::error(SerialComm::LogSource::Bluetooth, "Bluetooth queue create failed");
    return;
  }

#if BLUETOOTH_COMM_HAS_BLE
  BLEDevice::init(runtimeConfig.deviceName != nullptr ? runtimeConfig.deviceName : "PoolControl");
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BleServerCallbacks());

  BLEService *service = bleServer->createService(kServiceUuid);
  txCharacteristic = service->createCharacteristic(
    kTxUuid,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  txCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *rxCharacteristic = service->createCharacteristic(
    kRxUuid,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  rxCharacteristic->setCallbacks(new BleRxCallbacks());

  service->start();
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  transportAvailable = true;
  SerialComm::info(SerialComm::LogSource::Bluetooth, "BLE advertising started");
#elif BLUETOOTH_COMM_HAS_CLASSIC
  if (!serialBt.begin(runtimeConfig.deviceName != nullptr ? runtimeConfig.deviceName : "PoolControl")) {
    SerialComm::error(SerialComm::LogSource::Bluetooth, "Bluetooth start failed");
    return;
  }

  transportAvailable = true;
  SerialComm::info(SerialComm::LogSource::Bluetooth, "Classic Bluetooth started");
#else
  transportAvailable = false;
  SerialComm::warn(SerialComm::LogSource::Bluetooth, "No BLE or Classic Bluetooth stack available");
#endif

  BaseType_t created = xTaskCreate(
    bluetoothTask,
    "BluetoothTask",
    runtimeConfig.taskStackWords,
    nullptr,
    runtimeConfig.taskPriority,
    &bluetoothTaskHandle
  );

  ready = (created == pdPASS);
  if (ready) {
    SerialComm::info(SerialComm::LogSource::Bluetooth, "Bluetooth module started");
  } else {
    SerialComm::error(SerialComm::LogSource::Bluetooth, "Bluetooth task create failed");
  }
}

bool isReady() {
  return ready;
}

bool sendLine(const char *message) {
  return enqueueLine(message);
}

void setReceiveCallback(ReceiveCallback callback) {
  receiveCallback = callback;
}

}
