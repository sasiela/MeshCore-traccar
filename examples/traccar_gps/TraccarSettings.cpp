#include "TraccarSettings.h"
#include <stdio.h>
#include <string.h>

static int sensorSettingCount(const SensorManager& s) {
  return s.getNumSettings();
}

int TraccarSettings::getNumSettings() const {
  return sensorSettingCount(_sensors) + 6;
}

const char* TraccarSettings::getSettingName(int i) const {
  int n = sensorSettingCount(_sensors);
  if (i < n) {
    return _sensors.getSettingName(i);
  }
  i -= n;
  switch (i) {
    case 0: return "wifi_ssid";
    case 1: return "wifi_pwd";
    case 2: return "traccar_host";
    case 3: return "traccar_port";
    case 4: return "traccar_id";
    case 5: return "report_sec";
    default: return NULL;
  }
}

const char* TraccarSettings::getSettingValue(int i) const {
  int n = sensorSettingCount(_sensors);
  if (i < n) {
    return _sensors.getSettingValue(i);
  }
  i -= n;
  static char buf[32];
  switch (i) {
    case 0: return _cfg.wifi_ssid;
    case 1: return _cfg.wifi_pwd;
    case 2: return _cfg.traccar_host;
    case 3:
      snprintf(buf, sizeof(buf), "%u", (unsigned)_cfg.traccar_port);
      return buf;
    case 4: return _cfg.traccar_device_id;
    case 5:
      snprintf(buf, sizeof(buf), "%lu", (unsigned long)(_cfg.report_interval_ms / 1000));
      return buf;
    default: return NULL;
  }
}

bool TraccarSettings::setSettingValue(const char* name, const char* value) {
  if (_sensors.setSettingValue(name, value)) {
    return true;
  }
  if (strcmp(name, "wifi_ssid") == 0) {
    strncpy(_cfg.wifi_ssid, value, sizeof(_cfg.wifi_ssid) - 1);
    _cfg.wifi_ssid[sizeof(_cfg.wifi_ssid) - 1] = 0;
    _wifi_changed = true;
    _cfg.save();
    return true;
  }
  if (strcmp(name, "wifi_pwd") == 0) {
    strncpy(_cfg.wifi_pwd, value, sizeof(_cfg.wifi_pwd) - 1);
    _cfg.wifi_pwd[sizeof(_cfg.wifi_pwd) - 1] = 0;
    _wifi_changed = true;
    _cfg.save();
    return true;
  }
  if (strcmp(name, "traccar_host") == 0) {
    strncpy(_cfg.traccar_host, value, sizeof(_cfg.traccar_host) - 1);
    _cfg.traccar_host[sizeof(_cfg.traccar_host) - 1] = 0;
    _cfg.save();
    return true;
  }
  if (strcmp(name, "traccar_port") == 0) {
    int p = atoi(value);
    if (p > 0 && p < 65536) {
      _cfg.traccar_port = (uint16_t)p;
      _cfg.save();
      return true;
    }
    return false;
  }
  if (strcmp(name, "traccar_id") == 0) {
    strncpy(_cfg.traccar_device_id, value, sizeof(_cfg.traccar_device_id) - 1);
    _cfg.traccar_device_id[sizeof(_cfg.traccar_device_id) - 1] = 0;
    _cfg.save();
    return true;
  }
  if (strcmp(name, "report_sec") == 0) {
    long sec = atol(value);
    if (sec >= 5 && sec <= 86400) {
      _cfg.report_interval_ms = (uint32_t)sec * 1000;
      _cfg.save();
      return true;
    }
    return false;
  }
  return false;
}
