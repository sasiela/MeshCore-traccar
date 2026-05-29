#include "TraccarService.h"
#include <HTTPClient.h>
#include <WiFi.h>
#ifdef ESP32_PLATFORM
  #include <helpers/ESP32Board.h>
#endif
#if ENABLE_TRACCAR_WEB
  #include "TraccarWebServer.h"
#endif

TraccarConfig* TraccarService::_cfg = NULL;
TraccarSettings* TraccarService::_settings = NULL;
uint32_t TraccarService::_last_send_ms = 0;
uint32_t TraccarService::_last_wifi_attempt_ms = 0;
uint8_t TraccarService::_wifi_index = 0;
uint32_t TraccarService::_last_tx_attempt_ms = 0;
int TraccarService::_last_http_code = 0;
TraccarTxStatus TraccarService::_last_tx_status = TRACCAR_TX_NONE;
bool TraccarService::_ntp_done = false;

void TraccarService::bind(TraccarConfig& cfg, TraccarSettings& settings) {
  _cfg = &cfg;
  _settings = &settings;
#if ENABLE_TRACCAR_WEB
  TraccarWebServer::bind(cfg, settings);
#endif
}

int TraccarService::batteryPercent(uint16_t mv) {
  if (mv >= 4200) return 100;
  if (mv <= 3300) return 0;
  return (int)((mv - 3300) * 100 / 900);
}

bool TraccarService::hasBackupWifi() {
  return _cfg && _cfg->wifi_ssid_backup[0] != 0;
}

void TraccarService::wifiStartIndex(uint8_t index) {
  if (!_cfg) {
    return;
  }
  const char* ssid = (index == 0) ? _cfg->wifi_ssid : _cfg->wifi_ssid_backup;
  const char* pwd = (index == 0) ? _cfg->wifi_pwd : _cfg->wifi_pwd_backup;
  if (!ssid || ssid[0] == 0) {
    return;
  }
  _wifi_index = index;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, pwd);
  _last_wifi_attempt_ms = millis();
}

bool TraccarService::wifiEnsureConnected() {
  if (!_cfg || !_settings) {
    return false;
  }
  if (_settings->consumeWifiChanged()) {
    WiFi.disconnect();
    _wifi_index = 0;
    _last_wifi_attempt_ms = 0;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  uint32_t now = millis();
  if ((uint32_t)(now - _last_wifi_attempt_ms) < 10000) {
    return false;
  }
  if (_wifi_index == 0) {
    if (hasBackupWifi()) {
      wifiStartIndex(1);
    } else if (_cfg->wifi_ssid[0] != 0) {
      wifiStartIndex(0);
    }
  } else {
    wifiStartIndex(0);
  }
  return false;
}

bool TraccarService::sendPosition(mesh::MainBoard& board, SensorManager& sensors, int& http_code) {
  http_code = -1;
  if (!_cfg) {
    return false;
  }
#if !ENV_INCLUDE_GPS
  return false;
#else
  LocationProvider* loc = sensors.getLocationProvider();
  if (!loc || !loc->isValid()) {
    return false;
  }

  double lat = ((double)loc->getLatitude()) / 1000000.0;
  double lon = ((double)loc->getLongitude()) / 1000000.0;
  double alt = ((double)loc->getAltitude()) / 1000.0;
  long ts = loc->getTimestamp();
  if (ts <= 0) {
    ts = (long)time(nullptr);
  }
  if (ts <= 0) {
    ts = (long)(millis() / 1000);
  }

  float speed_knots = NAN;
  float bearing_deg = NAN;
  loc->getSpeedAndBearing(speed_knots, bearing_deg);

  char url[512];
  int n = snprintf(url, sizeof(url),
    "http://%s:%u/?id=%s&lat=%.6f&lon=%.6f&timestamp=%ld&altitude=%.1f&batt=%d",
    _cfg->traccar_host, (unsigned)_cfg->traccar_port, _cfg->traccar_device_id,
    lat, lon, (long)ts, alt, batteryPercent(board.getBattMilliVolts()));
  if (n < 0 || n >= (int)sizeof(url)) {
    return false;
  }
  if (!isnan(speed_knots) && speed_knots >= 0.0f) {
    int extra = snprintf(url + n, sizeof(url) - (size_t)n, "&speed=%.2f", speed_knots);
    if (extra > 0) {
      n += extra;
    }
  }
  if (!isnan(bearing_deg) && bearing_deg >= 0.0f && bearing_deg <= 360.0f) {
    int extra = snprintf(url + n, sizeof(url) - (size_t)n, "&bearing=%.1f", bearing_deg);
    if (extra > 0) {
      n += extra;
    }
  }
  if (n >= (int)sizeof(url)) {
    return false;
  }

  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(url)) {
    return false;
  }
  http_code = http.GET();
  http.end();
  return http_code > 0 && http_code < 400;
#endif
}

void TraccarService::begin(mesh::MainBoard& board) {
  if (!_cfg) {
    return;
  }
  _cfg->load();
  _cfg->save();
#ifdef ESP32_PLATFORM
  static_cast<ESP32Board&>(board).setInhibitSleep(true);
#endif
  if (!_ntp_done) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    _ntp_done = true;
  }
  if (_cfg->wifi_ssid[0] != 0) {
    wifiStartIndex(0);
  } else if (hasBackupWifi()) {
    wifiStartIndex(1);
  }
}

void TraccarService::loop(mesh::MainBoard& board, SensorManager& sensors) {
  if (!_cfg) {
    return;
  }
  if (!wifiEnsureConnected()) {
    _last_tx_status = TRACCAR_TX_WAIT_WIFI;
    return;
  }
#if ENABLE_TRACCAR_WEB
  TraccarWebServer::tick(board, sensors);
#endif
  uint32_t now = millis();
  if ((uint32_t)(now - _last_send_ms) < _cfg->report_interval_ms) {
    return;
  }
#if ENV_INCLUDE_GPS
  LocationProvider* loc = sensors.getLocationProvider();
  if (!loc || !loc->isValid()) {
    _last_tx_status = TRACCAR_TX_WAIT_GPS;
    return;
  }
#endif
  int code = -1;
  bool ok = sendPosition(board, sensors, code);
  _last_tx_attempt_ms = now;
  _last_http_code = code;
  _last_tx_status = ok ? TRACCAR_TX_OK : TRACCAR_TX_FAIL;
  if (ok) {
    _last_send_ms = now;
  }
}
