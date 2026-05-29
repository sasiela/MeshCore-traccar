#include "TraccarBleCompanion.h"
#include <helpers/esp32/SerialBLEInterface.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/TxtDataHelpers.h>
#include <helpers/AdvertDataHelpers.h>
#include <SPIFFS.h>
#include <esp_random.h>

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "Traccar-"
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "traccar-gps-1.0"
#endif
#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE __DATE__
#endif

#define CMD_APP_START             1
#define CMD_GET_BATT_AND_STORAGE  20
#define CMD_DEVICE_QEURY          22
#define CMD_REBOOT                19
#define CMD_SET_DEVICE_PIN        37
#define CMD_GET_CUSTOM_VARS       40
#define CMD_SET_CUSTOM_VAR        41
#define CMD_HAS_CONNECTION        28
#define CMD_SYNC_NEXT_MESSAGE     10
#define CMD_GET_CONTACTS          4

#define RESP_CODE_OK              0
#define RESP_CODE_ERR             1
#define RESP_CODE_SELF_INFO       5
#define RESP_CODE_BATT_AND_STORAGE 12
#define RESP_CODE_DEVICE_INFO     13
#define RESP_CODE_CUSTOM_VARS     21
#define RESP_CODE_SENT            3
#define RESP_CODE_CONTACT         4
#define RESP_CODE_END_OF_CONTACTS 6

#define ERR_CODE_UNSUPPORTED_CMD  1
#define ERR_CODE_ILLEGAL_ARG      6

#define FIRMWARE_VER_CODE         11
#define ADV_TYPE_SENSOR           4

static const char* IDENTITY_FILE = "/traccar_identity.bin";

TraccarBleCompanion::TraccarBleCompanion(mesh::MainBoard& board, TraccarConfig& cfg, TraccarSettings& settings,
                                         SensorManager& sensors)
  : _serial(NULL), _cfg(cfg), _settings(settings), _sensors(sensors), _board(board), _ble_pin(cfg.ble_pin) {}

bool TraccarBleCompanion::begin(SerialBLEInterface& ble) {
  if (SPIFFS.exists(IDENTITY_FILE)) {
    File f = SPIFFS.open(IDENTITY_FILE, "r");
    if (f) {
      uint8_t buf[PRV_KEY_SIZE];
      if (f.read(buf, sizeof(buf)) == sizeof(buf)) {
        _identity.readFrom(buf, PRV_KEY_SIZE);
      }
      f.close();
    }
  }
  if (_identity.pub_key[0] == 0 && _identity.pub_key[1] == 0) {
    StdRNG rng;
    rng.begin((uint32_t)esp_random());
    _identity = mesh::LocalIdentity(&rng);
    uint8_t prv[PRV_KEY_SIZE];
    size_t n = _identity.writeTo(prv, PRV_KEY_SIZE);
    File f = SPIFFS.open(IDENTITY_FILE, "w");
    if (f && n == PRV_KEY_SIZE) {
      f.write(prv, PRV_KEY_SIZE);
      f.close();
    }
  }

  char name[32];
  strncpy(name, _cfg.node_name, sizeof(name) - 1);
  name[sizeof(name) - 1] = 0;
  ble.begin(BLE_NAME_PREFIX, name, _ble_pin);
  ble.enable();
  _serial = &ble;
  return true;
}

void TraccarBleCompanion::writeOK() {
  uint8_t b = RESP_CODE_OK;
  _serial->writeFrame(&b, 1);
}

void TraccarBleCompanion::writeErr(uint8_t code) {
  uint8_t b[2] = { RESP_CODE_ERR, code };
  _serial->writeFrame(b, 2);
}

void TraccarBleCompanion::handleCmd(size_t len) {
  if (cmd_frame[0] == CMD_DEVICE_QEURY && len >= 2) {
    int i = 0;
    out_frame[i++] = RESP_CODE_DEVICE_INFO;
    out_frame[i++] = FIRMWARE_VER_CODE;
    out_frame[i++] = 0;
    out_frame[i++] = 0;
    memcpy(&out_frame[i], &_cfg.ble_pin, 4);
    i += 4;
    memset(&out_frame[i], 0, 12);
    strncpy((char*)&out_frame[i], FIRMWARE_BUILD_DATE, 11);
    i += 12;
    StrHelper::strzcpy((char*)&out_frame[i], _board.getManufacturerName(), 40);
    i += 40;
    StrHelper::strzcpy((char*)&out_frame[i], FIRMWARE_VERSION, 20);
    i += 20;
    out_frame[i++] = 0;
    out_frame[i++] = 0;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_APP_START && len >= 8) {
    cmd_frame[len] = 0;
    int i = 0;
    out_frame[i++] = RESP_CODE_SELF_INFO;
    out_frame[i++] = ADV_TYPE_SENSOR;
    out_frame[i++] = 0;
    out_frame[i++] = 22;
    memcpy(&out_frame[i], _identity.pub_key, PUB_KEY_SIZE);
    i += PUB_KEY_SIZE;
    int32_t lat = (int32_t)(_sensors.node_lat * 1000000.0);
    int32_t lon = (int32_t)(_sensors.node_lon * 1000000.0);
    memcpy(&out_frame[i], &lat, 4);
    i += 4;
    memcpy(&out_frame[i], &lon, 4);
    i += 4;
    out_frame[i++] = 0;
    out_frame[i++] = 0;
    out_frame[i++] = 0;
    out_frame[i++] = 0;
    uint32_t freq = (uint32_t)(869.618f * 1000);
    memcpy(&out_frame[i], &freq, 4);
    i += 4;
    uint32_t bw = (uint32_t)(62.5f * 1000);
    memcpy(&out_frame[i], &bw, 4);
    i += 4;
    out_frame[i++] = 8;
    out_frame[i++] = 5;
    int tlen = strlen(_cfg.node_name);
    memcpy(&out_frame[i], _cfg.node_name, tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_GET_BATT_AND_STORAGE) {
    uint8_t reply[11];
    int i = 0;
    reply[i++] = RESP_CODE_BATT_AND_STORAGE;
    uint16_t mv = _board.getBattMilliVolts();
    uint32_t used = 0;
    uint32_t total = 1024;
    memcpy(&reply[i], &mv, 2);
    i += 2;
    memcpy(&reply[i], &used, 4);
    i += 4;
    memcpy(&reply[i], &total, 4);
    i += 4;
    _serial->writeFrame(reply, i);
  } else if (cmd_frame[0] == CMD_GET_CUSTOM_VARS) {
    out_frame[0] = RESP_CODE_CUSTOM_VARS;
    char* dp = (char*)&out_frame[1];
    int max = (int)sizeof(out_frame) - 4;
    for (int j = 0; j < _settings.getNumSettings(); j++) {
      const char* nm = _settings.getSettingName(j);
      const char* val = _settings.getSettingValue(j);
      if (!nm || !val) continue;
      int need = (int)strlen(nm) + (int)strlen(val) + 2 + (dp - (char*)&out_frame[1]);
      if (need >= max) break;
      if (j > 0) {
        *dp++ = ',';
      }
      strcpy(dp, nm);
      dp += strlen(dp);
      *dp++ = ':';
      strcpy(dp, val);
      dp += strlen(dp);
    }
    _serial->writeFrame(out_frame, dp - (char*)out_frame);
  } else if (cmd_frame[0] == CMD_SET_CUSTOM_VAR && len >= 4) {
    cmd_frame[len] = 0;
    char* sp = (char*)&cmd_frame[1];
    char* np = strchr(sp, ':');
    if (np) {
      *np++ = 0;
      if (_settings.setSettingValue(sp, np)) {
        writeOK();
      } else {
        writeErr(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErr(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SET_DEVICE_PIN && len >= 5) {
    uint32_t pin;
    memcpy(&pin, &cmd_frame[1], 4);
    if (pin >= 100000 && pin <= 999999) {
      _cfg.ble_pin = pin;
      _ble_pin = pin;
      _cfg.save();
      writeOK();
    } else {
      writeErr(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_HAS_CONNECTION) {
    uint8_t b[2];
    b[0] = RESP_CODE_OK;
    b[1] = 0;
    _serial->writeFrame(b, 2);
  } else if (cmd_frame[0] == CMD_REBOOT) {
    writeOK();
    delay(100);
    _board.reboot();
  } else if (cmd_frame[0] == CMD_GET_CONTACTS) {
    out_frame[0] = RESP_CODE_END_OF_CONTACTS;
    memset(&out_frame[1], 0, 4);
    _serial->writeFrame(out_frame, 5);
  } else if (cmd_frame[0] == CMD_SYNC_NEXT_MESSAGE) {
    out_frame[0] = RESP_CODE_SENT;
    out_frame[1] = 0;
    _serial->writeFrame(out_frame, 2);
  } else {
    writeErr(ERR_CODE_UNSUPPORTED_CMD);
  }
}

void TraccarBleCompanion::loop() {
  if (!_serial) return;
  size_t len = _serial->checkRecvFrame(cmd_frame);
  if (len > 0) {
    handleCmd(len);
  }
}
