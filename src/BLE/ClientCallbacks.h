#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "BleKeyboardHost.h"

class ClientCallbacks : public NimBLEClientCallbacks {
    public:
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient, int reason) override;
    void onPassKeyEntry(NimBLEConnInfo& connInfo) override;
    void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pass_key) override;
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override;
    BleKeyboardHost* host;
    void setHost(BleKeyboardHost* host);
};