#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "BleKeyboardHost.h"

class ScanCallbacks : public NimBLEScanCallbacks {
    public:
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice);
    void onScanEnd(const NimBLEScanResults& results, int reason);
    BleKeyboardHost* host;
    void setHost(BleKeyboardHost* host);
};
