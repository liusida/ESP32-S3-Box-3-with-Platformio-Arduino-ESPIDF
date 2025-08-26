#pragma once

#include <Arduino.h>
#include <vector>
#include <queue>
#include <NimBLEDevice.h>

/**
 * BleKeyboardHost
 *
 * Scans for a BLE HID keyboard, connects, subscribes to Input Report
 * characteristics, logs reports, and (in the .cpp) forwards key events
 * into LVGL via a keypad input device.
 *
 * Usage:
 *   BleKeyboardHost keyboard;
 *   keyboard.begin();                            // sets up BLE + LVGL keypad indev
 *   keyboard.connect(callback, 500); // subscribe & start notifications
 *
 *   // Periodically:
 *   keyboard.pollLogs();                         // prints buffered logs to Serial
 *
 * Public helper:
 *   static char hidToAscii(uint8_t keycode, bool shift);
 */
class BleKeyboardHost {
public:
  /** 16-bit Bluetooth UUIDs used by HID keyboards */
  static const NimBLEUUID UUID_HID_SERVICE;  // 0x1812
  static const NimBLEUUID UUID_REPORT;       // 0x2A4D

  /** Singleton-like pointer to the current instance (set in begin()) */
  static BleKeyboardHost* instance;

  BleKeyboardHost();

  /** Initialize BLE host stack and set up the LVGL keypad indev (see .cpp). */
  void begin();

  /**
   * Scan for a HID device and connect.
   * @param callback  NimBLE notify callback invoked for each Input Report
   * @param duration  Scan duration in milliseconds
   */
  void connect(NimBLERemoteCharacteristic::notify_callback callback, uint32_t duration);

  /** True if connected and at least one Input Report characteristic is subscribed. */
  bool isReady() const;

  /** Drain and print the internal Serial logs generated from HID reports. */
  void pollLogs();

  /**
   * Log a received report and (implementation) enqueue LVGL key events.
   * You can call this from your app-level notify callback; connect() also calls
   * subscribeReports() which uses the provided callback.
   */
  void pushLog(NimBLERemoteCharacteristic *c, uint8_t *data, size_t len, bool isNotify);

  /**
   * Minimal HID keycode → ASCII mapping for printable keys.
   * Returns 0 if not a printable character.
   */
  static char hidToAscii(uint8_t kc, bool shift);

private:
  /** Discover HID service and subscribe to Input Report characteristics. */
  void subscribeReports(NimBLERemoteCharacteristic::notify_callback callback);

  NimBLEClient* client;
  std::vector<NimBLERemoteCharacteristic*> inputs;
  std::queue<String> logQueue;
};
