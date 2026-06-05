#include "main.h"
#include "FirebaseRTDB.h"
#include "BluetoothComm.h"
#include "WiFiManagerService.h"
#include "GitHubOTAService.h"

void createTasks();
void testSpi();
void readMCP23S17GPIOB();

namespace
{
  constexpr TickType_t statusDelay = pdMS_TO_TICKS(250);
  constexpr TickType_t heartbeatDelay = pdMS_TO_TICKS(500);

  void onFirebaseStreamEvent(
      const char *streamPath,
      const char *dataPath,
      const char *eventType,
      const char *dataType,
      const char *data)
  {
    char message[192] = {0};
    snprintf(
        message,
        sizeof(message),
        "Stream event path=%s dataPath=%s type=%s dataType=%s data=%s",
        streamPath != nullptr ? streamPath : "",
        dataPath != nullptr ? dataPath : "",
        eventType != nullptr ? eventType : "",
        dataType != nullptr ? dataType : "",
        data != nullptr ? data : "");
    SerialComm::info(SerialComm::LogSource::Firebase, message);
  }

  void onBluetoothMessage(const char *message)
  {
    if (message == nullptr)
    {
      return;
    }

    SerialComm::info(SerialComm::LogSource::Bluetooth, message);
  }


  void heartbeatTask(void *parameter)
  {
    (void)parameter;

    for (;;)
    {
      digitalWrite(Ledheartbeat, !digitalRead(Ledheartbeat));
      if (WiFiManagerService::isConnected()) {
        digitalWrite(LedWifi, HIGH);
      } else {
        digitalWrite(LedWifi, LOW);
      }
      vTaskDelay(heartbeatDelay);
    }
  }
}

void spiTask(void *parameter)
{
  (void)parameter;

  for (;;)
  {
    testSpi();
    SerialComm::info(SerialComm::LogSource::SPI, "FreeRTOS taak: SPI communicatie getest");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void createTasks()
{
  xTaskCreate(
      heartbeatTask,
      "HeartbeatTask",
      2048,
      nullptr,
      1,
      nullptr);

  xTaskCreate(
      spiTask,
      "TestSpiTask",
      4096,
      nullptr,
      1,
      nullptr);
}

void setup()
{
  SerialComm::begin(115200, 1, 24);

  WiFiManagerService::Config wifiConfig{};
  wifiConfig.enabled = WIFI_MANAGER_ENABLED;
  wifiConfig.portalApName = WIFI_MANAGER_AP_NAME;
  wifiConfig.portalApPassword = WIFI_MANAGER_AP_PASSWORD;
  wifiConfig.connectTimeoutSeconds = WIFI_MANAGER_CONNECT_TIMEOUT_S;
  WiFiManagerService::begin(wifiConfig);

  GitHubOTAService::Config otaConfig{};
  otaConfig.enabled = OTA_GITHUB_ENABLED;
  otaConfig.currentVersion = OTA_GITHUB_CURRENT_VERSION;
  otaConfig.versionUrl = OTA_GITHUB_VERSION_URL;
  otaConfig.firmwareUrl = OTA_GITHUB_FIRMWARE_URL;
  otaConfig.checkOnBoot = OTA_GITHUB_CHECK_ON_BOOT;
  otaConfig.checkIntervalSeconds = OTA_GITHUB_CHECK_INTERVAL_S;
  otaConfig.httpTimeoutMs = OTA_GITHUB_HTTP_TIMEOUT_MS;
  otaConfig.allowInsecureTls = OTA_GITHUB_ALLOW_INSECURE_TLS;
  GitHubOTAService::begin(otaConfig);

  if (FIREBASE_ENABLED) {
    FirebaseRTDB::Config firebaseConfig{};
    firebaseConfig.enabled = true;
    firebaseConfig.apiKey = FIREBASE_API_KEY;
    firebaseConfig.databaseUrl = FIREBASE_DATABASE_URL;
    firebaseConfig.userEmail = FIREBASE_USER_EMAIL;
    firebaseConfig.userPassword = FIREBASE_USER_PASSWORD;
    FirebaseRTDB::begin(firebaseConfig);
    FirebaseRTDB::setStreamCallback(onFirebaseStreamEvent);
    FirebaseRTDB::startStream("/pool/commands");
    FirebaseRTDB::writeBool("/pool/system/booted", true);
  }

  BluetoothComm::Config bluetoothConfig{};
  bluetoothConfig.enabled = BLUETOOTH_ENABLED;
  bluetoothConfig.deviceName = BLUETOOTH_DEVICE_NAME;
  BluetoothComm::setReceiveCallback(onBluetoothMessage);
  BluetoothComm::begin(bluetoothConfig);
  BluetoothComm::sendLine("PoolControl BLE ready");

  configurePins();
  setup_Spi();
  createTasks();
}

void loop()
{
  vTaskDelay(portMAX_DELAY);
}
