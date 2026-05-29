#include "TraccarConfig.h"
#include <SPIFFS.h>
#include <string.h>
#if defined(ESP32_PLATFORM)
  #include <esp_mac.h>
#endif

static const char* PREFS_FILE = "/traccar_prefs.bin";

#ifndef WIFI_SSID_BACKUP
#define WIFI_SSID_BACKUP "SMART_HOME"
#endif
#ifndef WIFI_PWD_BACKUP
#define WIFI_PWD_BACKUP "domoticz5"
#endif

// Layout before backup Wi-Fi fields (SPIFFS migration).
struct TraccarConfigV1 {
  char wifi_ssid[33];
  char wifi_pwd[65];
  char traccar_host[64];
  uint16_t traccar_port;
  char traccar_device_id[32];
  uint32_t report_interval_ms;
  uint32_t ble_pin;
  char node_name[32];
};

void TraccarConfig::setDefaults() {
#ifndef WIFI_SSID
#define WIFI_SSID "CAR"
#endif
#ifndef WIFI_PWD
#define WIFI_PWD "kalafior6"
#endif
#ifndef TRACCAR_HOST
#define TRACCAR_HOST "tracking.sasiela.pl"
#endif
#ifndef TRACCAR_PORT
#define TRACCAR_PORT 5055
#endif
#ifndef TRACCAR_INTERVAL_MS
#define TRACCAR_INTERVAL_MS 30000
#endif
#ifndef BLE_PIN_CODE
#define BLE_PIN_CODE 409494
#endif

  strncpy(wifi_ssid, WIFI_SSID, sizeof(wifi_ssid) - 1);
  strncpy(wifi_pwd, WIFI_PWD, sizeof(wifi_pwd) - 1);
  strncpy(wifi_ssid_backup, WIFI_SSID_BACKUP, sizeof(wifi_ssid_backup) - 1);
  strncpy(wifi_pwd_backup, WIFI_PWD_BACKUP, sizeof(wifi_pwd_backup) - 1);
  strncpy(traccar_host, TRACCAR_HOST, sizeof(traccar_host) - 1);
  traccar_port = TRACCAR_PORT;
  setDeviceIdFromBleMac();
  report_interval_ms = TRACCAR_INTERVAL_MS;
  ble_pin = BLE_PIN_CODE;
  strncpy(node_name, "Traccar", sizeof(node_name) - 1);
}

void TraccarConfig::setDeviceIdFromBleMac() {
#if defined(ESP32_PLATFORM)
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_BT) != ESP_OK) {
    esp_efuse_mac_get_default(mac);
  }
  snprintf(traccar_device_id, sizeof(traccar_device_id),
           "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#else
  strncpy(traccar_device_id, "node", sizeof(traccar_device_id) - 1);
#endif
  traccar_device_id[sizeof(traccar_device_id) - 1] = 0;
}

bool TraccarConfig::load() {
  setDefaults();
  if (!SPIFFS.exists(PREFS_FILE)) {
    return false;
  }
  File f = SPIFFS.open(PREFS_FILE, "r");
  if (!f) {
    return false;
  }
  size_t file_size = f.size();
  bool ok = false;
  if (file_size == sizeof(TraccarConfig)) {
    ok = f.read((uint8_t*)this, sizeof(TraccarConfig)) == sizeof(TraccarConfig);
  } else if (file_size == sizeof(TraccarConfigV1)) {
    TraccarConfigV1 old_cfg;
    ok = f.read((uint8_t*)&old_cfg, sizeof(old_cfg)) == sizeof(old_cfg);
    if (ok) {
      memcpy(wifi_ssid, old_cfg.wifi_ssid, sizeof(wifi_ssid));
      memcpy(wifi_pwd, old_cfg.wifi_pwd, sizeof(wifi_pwd));
      wifi_ssid_backup[0] = 0;
      wifi_pwd_backup[0] = 0;
      memcpy(traccar_host, old_cfg.traccar_host, sizeof(traccar_host));
      traccar_port = old_cfg.traccar_port;
      memcpy(traccar_device_id, old_cfg.traccar_device_id, sizeof(traccar_device_id));
      report_interval_ms = old_cfg.report_interval_ms;
      ble_pin = old_cfg.ble_pin;
      memcpy(node_name, old_cfg.node_name, sizeof(node_name));
    }
  }
  f.close();
  if (traccar_port == 0) {
    traccar_port = 5055;
  }
  if (report_interval_ms < 5000) {
    report_interval_ms = 30000;
  }
  wifi_ssid[sizeof(wifi_ssid) - 1] = 0;
  wifi_pwd[sizeof(wifi_pwd) - 1] = 0;
  wifi_ssid_backup[sizeof(wifi_ssid_backup) - 1] = 0;
  wifi_pwd_backup[sizeof(wifi_pwd_backup) - 1] = 0;
  traccar_host[sizeof(traccar_host) - 1] = 0;
  traccar_device_id[sizeof(traccar_device_id) - 1] = 0;
  node_name[sizeof(node_name) - 1] = 0;
  if (!ok || traccar_device_id[0] == 0 || strcmp(traccar_device_id, "car1") == 0) {
    setDeviceIdFromBleMac();
  }
  if (wifi_ssid_backup[0] == 0) {
    strncpy(wifi_ssid_backup, WIFI_SSID_BACKUP, sizeof(wifi_ssid_backup) - 1);
    strncpy(wifi_pwd_backup, WIFI_PWD_BACKUP, sizeof(wifi_pwd_backup) - 1);
    wifi_ssid_backup[sizeof(wifi_ssid_backup) - 1] = 0;
    wifi_pwd_backup[sizeof(wifi_pwd_backup) - 1] = 0;
  }
  return ok;
}

bool TraccarConfig::save() const {
  File f = SPIFFS.open(PREFS_FILE, "w");
  if (!f) {
    return false;
  }
  size_t n = f.write((const uint8_t*)this, sizeof(TraccarConfig));
  f.close();
  return n == sizeof(TraccarConfig);
}
