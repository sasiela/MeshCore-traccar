#pragma once

#include <helpers/SensorManager.h>
#include "TraccarConfig.h"

class TraccarSettings : public SensorManager {
  TraccarConfig& _cfg;
  SensorManager& _sensors;
  bool _wifi_changed;

public:
  TraccarSettings(TraccarConfig& cfg, SensorManager& sensors)
    : _cfg(cfg), _sensors(sensors), _wifi_changed(false) {}

  bool consumeWifiChanged() {
    bool v = _wifi_changed;
    _wifi_changed = false;
    return v;
  }

  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
  LocationProvider* getLocationProvider() override {
    return _sensors.getLocationProvider();
  }
};
