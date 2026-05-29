/**
 * Standalone GPS tracker: minimal BLE config + WiFi -> Traccar.
 * For full Mesh LoRa companion use Heltec_v3_companion_traccar_ble instead.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <target.h>
#include <helpers/esp32/SerialBLEInterface.h>
#include "TraccarConfig.h"
#include "TraccarSettings.h"
#include "TraccarBleCompanion.h"
#include "TraccarService.h"

static TraccarConfig traccar_cfg;
static TraccarSettings traccar_settings(traccar_cfg, sensors);
static SerialBLEInterface ble_interface;
static TraccarBleCompanion* ble_companion = NULL;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("MeshCore Traccar GPS (tracker-only)");

  board.begin();
  SPIFFS.begin(true);

  rtc_clock.begin(Wire);

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.print("Traccar GPS");
    display.endFrame();
  }
#endif

#if ENV_INCLUDE_GPS
  sensors.begin();
  sensors.setSettingValue("gps", "1");
  LocationProvider* loc = sensors.getLocationProvider();
  if (loc) {
    loc->begin();
    loc->reset();
  }
#endif

  TraccarService::bind(traccar_cfg, traccar_settings);
  TraccarService::begin(board);

  ble_companion = new TraccarBleCompanion(board, traccar_cfg, traccar_settings, sensors);
  ble_companion->begin(ble_interface);
}

void loop() {
  if (ble_companion) {
    ble_companion->loop();
  }

#if ENV_INCLUDE_GPS
  LocationProvider* loc = sensors.getLocationProvider();
  if (loc) {
    loc->loop();
    if (loc->isValid()) {
      sensors.node_lat = ((double)loc->getLatitude()) / 1000000.0;
      sensors.node_lon = ((double)loc->getLongitude()) / 1000000.0;
      sensors.node_altitude = ((double)loc->getAltitude()) / 1000.0;
    }
  }
#endif

#ifdef DISPLAY_CLASS
  static uint32_t next_ui = 0;
  if ((uint32_t)(millis() - next_ui) > 2000) {
    next_ui = millis();
    display.startFrame();
    display.setCursor(0, 0);
    display.print(ble_interface.isConnected() ? "BLE on" : "BLE adv");
    display.print(WiFi.status() == WL_CONNECTED ? "\nWiFi OK" : "\nWiFi ..");
#if ENV_INCLUDE_GPS
    LocationProvider* loc = sensors.getLocationProvider();
    display.print(loc && loc->isValid() ? "\nGPS fix" : "\nGPS wait");
#endif
    display.endFrame();
  }
#endif

  TraccarService::loop(board, sensors);
  delay(20);
}
