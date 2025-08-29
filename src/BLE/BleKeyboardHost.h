#pragma once

#include <Arduino.h>
#include <queue>
#include <NimBLEDevice.h>

#define UUID_HID_SERVICE NimBLEUUID((uint16_t)0x1812)
#define UUID_REPORT NimBLEUUID((uint16_t)0x2A4D)
#define APPEARANCE_KEYBOARD ((uint16_t)0x03C1)

struct KeyEvent {
    uint16_t keycode;
    bool pressed;
    uint32_t timestamp;
};

class BleKeyboardHost {
public:
    BleKeyboardHost();
    void begin(uint32_t scanTimeMs = 1000);
    void tick();
    void setNotifyCB(void (*notifyCB)(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify));

    bool connectToServer();

    // New methods for key handling
    bool hasKey();
    KeyEvent getKey();
    void parseHIDReport(uint8_t* data, size_t length);
    uint16_t convertHIDToLVGL(uint8_t hidKey);

    bool m_doConnect;
    bool m_isConnected;
    const NimBLEAdvertisedDevice *m_advDevice;
    uint32_t m_scanTimeMs;

private:
    std::queue<KeyEvent> keyQueue;
    void (*m_notifyCB)(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
};
