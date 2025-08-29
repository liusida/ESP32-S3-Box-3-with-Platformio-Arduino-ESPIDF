#include "BleKeyboardHost.h"
#include "ClientCallbacks.h"
#include "ScanCallbacks.h"
#include "NimBLELog.h"
#include "lvgl.h"

static const char* LOG_TAG = "BLEKeyboardHost";

ClientCallbacks clientCallbacks;
ScanCallbacks scanCallbacks;

void defaultNotifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic,
                     uint8_t *pData, size_t length, bool isNotify) {
  NIMBLE_LOGI(LOG_TAG, "notification received, please implement your own callback");
}

BleKeyboardHost::BleKeyboardHost()
    : m_doConnect(false), m_isConnected(false), m_scanTimeMs(0),
      m_advDevice(nullptr), m_notifyCB(defaultNotifyCB) {}

void BleKeyboardHost::begin(uint32_t scanTimeMs) {
  m_scanTimeMs = scanTimeMs;
  NimBLEDevice::init("NimBLE-Client");
  NimBLEDevice::setPower(3); /** 3dbm */
  NimBLEScan *pScan = NimBLEDevice::getScan();
  
  // Create scan callbacks with pointer to this host
  scanCallbacks.setHost(this);
  pScan->setScanCallbacks(&scanCallbacks, false);
  pScan->setInterval(100);
  pScan->setWindow(100);
}

void BleKeyboardHost::tick() {
  if (m_doConnect) {
    m_doConnect = false;
    /** Found a device we want to connect to, do it now */
    if (connectToServer()) {
      m_isConnected = true;
      NIMBLE_LOGI(LOG_TAG, "Success! we should now be getting notifications!");
    } else {
      NIMBLE_LOGE(LOG_TAG, "Failed to connect, starting scan");
    }
  }
  if (!m_isConnected) {
    if (!NimBLEDevice::getScan()->isScanning()) {
      NIMBLE_LOGI(LOG_TAG, "No connection, scanning for peripherals");
      NimBLEDevice::getScan()->start(m_scanTimeMs, false, true);
    }
  }
}

void BleKeyboardHost::setNotifyCB(
    void (*notifyCB)(NimBLERemoteCharacteristic *pRemoteCharacteristic,
                     uint8_t *pData, size_t length, bool isNotify)) {
  m_notifyCB = notifyCB;
}

bool BleKeyboardHost::connectToServer() {
  NimBLEClient *pClient = nullptr;

  /** Check if we have a client we should reuse first **/
  if (NimBLEDevice::getCreatedClientCount()) {
    /**
     *  Special case when we already know this device, we send false as the
     *  second argument in connect() to prevent refreshing the service database.
     *  This saves considerable time and power.
     */
    pClient = NimBLEDevice::getClientByPeerAddress(m_advDevice->getAddress());
    if (pClient) {
      if (!pClient->connect(m_advDevice, false)) {
        NIMBLE_LOGE(LOG_TAG, "Reconnect failed");
        return false;
      }
      NIMBLE_LOGI(LOG_TAG, "Reconnected client");
    } else {
      /**
       *  We don't already have a client that knows this device,
       *  check for a client that is disconnected that we can use.
       */
      pClient = NimBLEDevice::getDisconnectedClient();
    }
  }

  /** No client to reuse? Create a new one. */
  if (!pClient) {
    if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
      NIMBLE_LOGE(LOG_TAG, "Max clients reached - no more connections available");
      return false;
    }

    pClient = NimBLEDevice::createClient();

    NIMBLE_LOGI(LOG_TAG, "New client created");

    clientCallbacks.setHost(this);
    pClient->setClientCallbacks(&clientCallbacks, false);
    /**
     *  Set initial connection parameters:
     *  These settings are safe for 3 clients to connect reliably, can go faster
     * if you have less connections. Timeout should be a multiple of the
     * interval, minimum is 100ms. Min interval: 12 * 1.25ms = 15, Max interval:
     * 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
     */
    pClient->setConnectionParams(12, 12, 0, 150);

    /** Set how long we are willing to wait for the connection to complete
     * (milliseconds), default is 30000. */
    pClient->setConnectTimeout(5 * 1000);

    if (!pClient->connect(m_advDevice)) {
      /** Created a client but failed to connect, don't need to keep it as it
       * has no data */
      NimBLEDevice::deleteClient(pClient);
      NIMBLE_LOGE(LOG_TAG, "Failed to connect, deleted client");
      return false;
    }
  }

  if (!pClient->isConnected()) {
    if (!pClient->connect(m_advDevice)) {
      NIMBLE_LOGE(LOG_TAG, "Failed to connect");
      return false;
    }
  }

  NIMBLE_LOGI(LOG_TAG, "Connected to: %s RSSI: %d",
                pClient->getPeerAddress().toString().c_str(),
                pClient->getRssi());

  pClient->secureConnection(); // This fixes write error 261 (insufficient
                               // auth)! or in ESP-IDF stack, call
                               // `ble_gap_security_initiate(uint16_t
                               // conn_handle);`

  /** Now we can read/write/subscribe the characteristics of the services we are
   * interested in */
  NimBLERemoteService *pSvc = nullptr;
  NimBLERemoteCharacteristic *pChr = nullptr;
  NimBLERemoteDescriptor *pDsc = nullptr;

  pSvc = pClient->getService(UUID_HID_SERVICE);
  if (pSvc) {
    // Get ALL characteristics with UUID_REPORT (there might be multiple for
    // different report types)
    const std::vector<NimBLERemoteCharacteristic *> characteristics =
        pSvc->getCharacteristics(true);
    if (characteristics.size() > 0) {
      NIMBLE_LOGI(LOG_TAG, "Found %d characteristics with UUID_REPORT",
                    characteristics.size());

      for (auto chr : characteristics) {
        NIMBLE_LOGI(LOG_TAG,
            "Characteristic handle: %d, canNotify: %s, canIndicate: %s\n",
            chr->getHandle(), chr->canNotify() ? "true" : "false",
            chr->canIndicate() ? "true" : "false");

        if (chr->canNotify()) {
          if (!chr->subscribe(true, m_notifyCB)) {
            NIMBLE_LOGE(LOG_TAG, "Failed to subscribe to notifications on handle %d",
                          chr->getHandle());
            pClient->disconnect();
            return false;
          }
          NIMBLE_LOGI(LOG_TAG, "Subscribed to notifications on handle %d",
                        chr->getHandle());
        } else if (chr->canIndicate()) {
          if (!chr->subscribe(false, m_notifyCB)) {
            NIMBLE_LOGE(LOG_TAG, "Failed to subscribe to indications on handle %d",
                          chr->getHandle());
            pClient->disconnect();
            return false;
          }
          NIMBLE_LOGI(LOG_TAG, "Subscribed to indications on handle %d",
                        chr->getHandle());
        }
      }
    } else {
      NIMBLE_LOGE(LOG_TAG, "No characteristics found with UUID_REPORT");
      pClient->disconnect();
      return false;
    }
  } else {
    NIMBLE_LOGE(LOG_TAG, "UUID_HID_SERVICE service not found.");
  }

  NIMBLE_LOGI(LOG_TAG, "Done with this device!");
  return true;
}

bool BleKeyboardHost::hasKey() {
  return !keyQueue.empty();
}

KeyEvent BleKeyboardHost::getKey() {
  if (keyQueue.empty()) {
      return {0, false, 0};
  }
  
  KeyEvent key = keyQueue.front();
  keyQueue.pop();
  return key;
}

void BleKeyboardHost::parseHIDReport(uint8_t* data, size_t length) {
  if (length < 8) return; // HID keyboard reports are typically 8 bytes
  
  // Standard HID keyboard report format:
  // Byte 0: Modifier keys (Ctrl, Shift, Alt, etc.)
  // Byte 1: Reserved
  // Byte 2-7: Key codes (up to 6 keys can be pressed simultaneously)
  
  uint8_t modifiers = data[0];
  uint8_t keycodes[6];
  memcpy(keycodes, &data[2], 6);
  
  // // Process modifier keys
  // if (modifiers & 0x01) keyQueue.push({LV_KEY_CTRL, true, millis()});
  // if (modifiers & 0x02) keyQueue.push({LV_KEY_SHIFT, true, millis()});
  // if (modifiers & 0x04) keyQueue.push({LV_KEY_ALT, true, millis()});
  // if (modifiers & 0x08) keyQueue.push({LV_KEY_GUI, true, millis()});
  
  // Process regular key codes
  for (int i = 0; i < 6; i++) {
      if (keycodes[i] != 0) {
          // Convert HID key codes to LVGL key codes
          uint16_t lvglKey = convertHIDToLVGL(keycodes[i]);
          if (lvglKey != 0) {
              keyQueue.push({lvglKey, true, millis()});
              keyQueue.push({lvglKey, false, millis() + 1});
          }
      }
  }
  
  // Note: For a complete implementation, you'd also need to track key releases
  // This would require maintaining a state of currently pressed keys
}

// Helper function to convert HID key codes to LVGL key codes
uint16_t BleKeyboardHost::convertHIDToLVGL(uint8_t hidKey) {
  // Basic mapping - you can expand this based on your needs
  switch (hidKey) {
      case 0x04: return 'a'; // A
      case 0x05: return 'b'; // B
      case 0x06: return 'c'; // C
      case 0x07: return 'd'; // D
      case 0x08: return 'e'; // E
      case 0x09: return 'f'; // F
      case 0x0A: return 'g'; // G
      case 0x0B: return 'h'; // H
      case 0x0C: return 'i'; // I
      case 0x0D: return 'j'; // J
      case 0x0E: return 'k'; // K
      case 0x0F: return 'l'; // L
      case 0x10: return 'm'; // M
      case 0x11: return 'n'; // N
      case 0x12: return 'o'; // O
      case 0x13: return 'p'; // P
      case 0x14: return 'q'; // Q
      case 0x15: return 'r'; // R
      case 0x16: return 's'; // S
      case 0x17: return 't'; // T
      case 0x18: return 'u'; // U
      case 0x19: return 'v'; // V
      case 0x1A: return 'w'; // W
      case 0x1B: return 'x'; // X
      case 0x1C: return 'y'; // Y
      case 0x1D: return 'z'; // Z
      case 0x1E: return '1'; // 1
      case 0x1F: return '2'; // 2
      case 0x20: return '3'; // 3
      case 0x21: return '4'; // 4
      case 0x22: return '5'; // 5
      case 0x23: return '6'; // 6
      case 0x24: return '7'; // 7
      case 0x25: return '8'; // 8
      case 0x26: return '9'; // 9
      case 0x27: return '0'; // 0
      case 0x28: return LV_KEY_ENTER; // Enter
      case 0x29: return LV_KEY_ESC; // Escape
      case 0x2A: return LV_KEY_BACKSPACE; // Backspace
      case 0x2B: return '\t'; // Tab
      case 0x2C: return ' '; // Space
      case 0x2D: return '-'; // Minus
      case 0x2E: return '='; // Equal
      case 0x2F: return '['; // Left bracket
      case 0x30: return ']'; // Right bracket
      case 0x31: return '\\'; // Backslash
      case 0x32: return '#'; // Hash
      case 0x33: return ';'; // Semicolon
      case 0x34: return '\''; // Quote
      case 0x35: return '`'; // Grave
      case 0x36: return ','; // Comma
      case 0x37: return '.'; // Period
      case 0x38: return '/'; // Slash
      // case 0x39: return LV_KEY_CAPS_LOCK; // Caps Lock
      case 0x4A: return LV_KEY_HOME; // Home
      case 0x4B: return LV_KEY_END; // End
      case 0x4C: return LV_KEY_DEL; // Delete
      case 0x4D: return LV_KEY_RIGHT; // Right Arrow
      case 0x4E: return LV_KEY_LEFT; // Left Arrow
      case 0x4F: return LV_KEY_DOWN; // Down Arrow
      case 0x50: return LV_KEY_UP; // Up Arrow
      default: return 0; // Unknown key
  }
}