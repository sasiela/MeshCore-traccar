#pragma once

#include <Arduino.h>

struct TraccarConfig {
  char wifi_ssid[33];
  char wifi_pwd[65];
  char wifi_ssid_backup[33];
  char wifi_pwd_backup[65];
  char traccar_host[64];
  uint16_t traccar_port;
  char traccar_device_id[32];
  uint32_t report_interval_ms;
  uint32_t ble_pin;
  char node_name[32];

  void setDefaults();
  void setDeviceIdFromBleMac();
  bool load();
  bool save() const;
};
