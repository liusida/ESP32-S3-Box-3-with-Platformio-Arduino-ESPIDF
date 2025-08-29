#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

class BleKeyboardHost {
public:
    // Start the background process
    void begin();
    
    // Check if background process is complete
    bool isReady() const;
    
    // Get current step status
    enum class Step {
        IDLE,
        SCANNING,
        FILTERING_DEVICES,
        CONNECTING,
        DISCOVERING_SERVICES,
        SUBSCRIBING,
        READY,
        ERROR
    };
    
    Step getCurrentStep() const;
    
    // Manual control (optional)
    void reset();
    
    // Data access
    std::vector<BleDevice> getDiscoveredDevices() const;
    bool isConnected() const;
    KeyEvent getNextKeyEvent();  // Get keyboard input
};
