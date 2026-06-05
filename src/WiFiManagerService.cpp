#include "WiFiManagerService.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include "config.h"

#include "SerialComm.h"

namespace {

bool wifiReady = false;
WiFiManagerService::Config runtimeConfig{};

}

namespace WiFiManagerService {

void begin(const Config &config) {
  if (wifiReady) {
    return;
  }

  runtimeConfig = config;
  if (!runtimeConfig.enabled) {
    SerialComm::warn(SerialComm::LogSource::System, "WiFi manager is disabled");
    return;
  }

  WiFi.mode(WIFI_STA);

  WiFiManager wifiManager;
  wifiManager.setConnectTimeout(runtimeConfig.connectTimeoutSeconds);
  wifiManager.setConfigPortalBlocking(true);
  wifiManager.setBreakAfterConfig(runtimeConfig.autoRestartOnSave);

  const char *apPassword = runtimeConfig.portalApPassword;
  if (apPassword != nullptr && apPassword[0] != '\0') {
    if (!wifiManager.autoConnect(runtimeConfig.portalApName, apPassword)) {
      SerialComm::error(SerialComm::LogSource::System, "WiFi setup failed");
      return;
    }
  } else {
    if (!wifiManager.autoConnect(runtimeConfig.portalApName)) {
      SerialComm::error(SerialComm::LogSource::System, "WiFi setup failed");
      return;
    }
  }

  wifiReady = (WiFi.status() == WL_CONNECTED);
  if (wifiReady) {
    String ip = WiFi.localIP().toString();
    char message[96] = {0};
    snprintf(message, sizeof(message), "WiFi connected, IP=%s", ip.c_str());
    SerialComm::info(SerialComm::LogSource::System, message);
  }
}

bool isReady() {
  return wifiReady;
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String localIp() {
  if (!isConnected()) {
    return String();
  }
  return WiFi.localIP().toString();
}

}
