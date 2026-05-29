#pragma once

#include <helpers/BaseSerialInterface.h>
#include <helpers/esp32/SerialBLEInterface.h>
#include <Mesh.h>
#include "TraccarConfig.h"
#include "TraccarSettings.h"

class TraccarBleCompanion {
  BaseSerialInterface* _serial;
  TraccarConfig& _cfg;
  TraccarSettings& _settings;
  SensorManager& _sensors;
  mesh::MainBoard& _board;
  mesh::LocalIdentity _identity;
  uint32_t _ble_pin;
  uint8_t cmd_frame[MAX_FRAME_SIZE];
  uint8_t out_frame[MAX_FRAME_SIZE];

  void writeOK();
  void writeErr(uint8_t code);
  void handleCmd(size_t len);

public:
  TraccarBleCompanion(mesh::MainBoard& board, TraccarConfig& cfg, TraccarSettings& settings, SensorManager& sensors);

  bool begin(SerialBLEInterface& ble);
  void loop();
  uint32_t getBlePin() const { return _ble_pin; }
};
