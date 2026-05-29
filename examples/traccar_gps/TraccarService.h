#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/SensorManager.h>
#include "TraccarConfig.h"
#include "TraccarSettings.h"

enum TraccarTxStatus : uint8_t {
  TRACCAR_TX_NONE = 0,
  TRACCAR_TX_OK,
  TRACCAR_TX_FAIL,
  TRACCAR_TX_WAIT_WIFI,
  TRACCAR_TX_WAIT_GPS,
};

class TraccarService {
  static TraccarConfig* _cfg;
  static TraccarSettings* _settings;
  static uint32_t _last_send_ms;
  static uint32_t _last_wifi_attempt_ms;
  static uint8_t _wifi_index;
  static uint32_t _last_tx_attempt_ms;
  static int _last_http_code;
  static TraccarTxStatus _last_tx_status;
  static bool _ntp_done;

  static int batteryPercent(uint16_t mv);
  static void wifiStartIndex(uint8_t index);
  static bool wifiEnsureConnected();
  static bool hasBackupWifi();
  static bool sendPosition(mesh::MainBoard& board, SensorManager& sensors, int& http_code);

public:
  static void bind(TraccarConfig& cfg, TraccarSettings& settings);
  static void begin(mesh::MainBoard& board);
  static void loop(mesh::MainBoard& board, SensorManager& sensors);

  static TraccarTxStatus getTxStatus() { return _last_tx_status; }
  static int getLastHttpCode() { return _last_http_code; }
  static uint32_t getLastTxAttemptMs() { return _last_tx_attempt_ms; }
  static const TraccarConfig* getConfig() { return _cfg; }
  /** 0 = primary (wifi_ssid), 1 = backup (wifi_ssid2); valid only while connected. */
  static int getActiveWifiIndex() { return (int)_wifi_index; }
};
