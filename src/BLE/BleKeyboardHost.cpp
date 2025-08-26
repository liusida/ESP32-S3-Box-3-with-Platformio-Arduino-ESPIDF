// BleKeyboardHost.cpp 
// Windows-like HID key handling for LVGL 8.3.11: emit PRESS on first appearance,
// emit RELEASE when the key disappears; no synthetic extra characters.

#include "BleKeyboardHost.h"

#include <Arduino.h>
#include <lvgl.h>
#include <vector>
#include <queue>
#include <cctype>
#include <cstdio>
#include <cstring>

// FreeRTOS critical section for a tiny lock
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// =====================
// Static members
// =====================
const NimBLEUUID BleKeyboardHost::UUID_HID_SERVICE((uint16_t)0x1812);
const NimBLEUUID BleKeyboardHost::UUID_REPORT((uint16_t)0x2A4D);
BleKeyboardHost* BleKeyboardHost::instance = nullptr;

// =====================
// Local LVGL input glue
// =====================

struct KeyEvt { uint32_t key; bool pressed; };

// Simple ring buffer for key events
static KeyEvt key_rb[128];
static volatile uint16_t key_head = 0;
static volatile uint16_t key_tail = 0;
static portMUX_TYPE key_mux = portMUX_INITIALIZER_UNLOCKED;

static inline bool key_rb_empty() {
  return key_head == key_tail;
}
static inline bool key_rb_full() {
  return ((key_head + 1) & (uint16_t)(128 - 1)) == key_tail;
}
static void key_enqueue(uint32_t key, bool pressed) {
  portENTER_CRITICAL(&key_mux);
  if (!key_rb_full()) {
    key_rb[key_head] = KeyEvt{key, pressed};
    key_head = (key_head + 1) & (uint16_t)(128 - 1);
  } else {
    // drop oldest on overflow
    key_tail = (key_tail + 1) & (uint16_t)(128 - 1);
    key_rb[key_head] = KeyEvt{key, pressed};
    key_head = (key_head + 1) & (uint16_t)(128 - 1);
  }
  portEXIT_CRITICAL(&key_mux);
}
static bool key_dequeue(KeyEvt* out) {
  bool ok = false;
  portENTER_CRITICAL(&key_mux);
  if (!key_rb_empty()) {
    *out = key_rb[key_tail];
    key_tail = (key_tail + 1) & (uint16_t)(128 - 1);
    ok = true;
  }
  portEXIT_CRITICAL(&key_mux);
  return ok;
}

// Map HID usage -> LVGL key (printables return their Unicode codepoint)
static uint32_t hid_to_lv_key(uint8_t kc, bool shift) {
  char ch = BleKeyboardHost::hidToAscii(kc, shift);
  if (ch) return (uint32_t)ch;

  switch (kc) {
    case 0x28: return LV_KEY_ENTER;
    case 0x2A: return LV_KEY_BACKSPACE;
    case 0x2B: return LV_KEY_NEXT;       // Tab
    case 0x29: return LV_KEY_ESC;
    case 0x4F: return LV_KEY_RIGHT;
    case 0x50: return LV_KEY_LEFT;
    case 0x51: return LV_KEY_DOWN;
    case 0x52: return LV_KEY_UP;
    default:   return 0;
  }
}

// ===============
// Keyboard state
// ===============
static uint8_t prev_mod = 0;
static uint8_t prev_keys[6] = {0};

struct PressedEntry { uint8_t kc; uint32_t lvkey; };
static PressedEntry pressed_map[6] = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};

static inline bool kc_in(const uint8_t *arr6, uint8_t kc) {
  if (kc == 0) return false;
  for (int i = 0; i < 6; ++i) if (arr6[i] == kc) return true;
  return false;
}
static inline int pressed_slot_for(uint8_t kc) {
  for (int i=0;i<6;++i) if (pressed_map[i].kc == kc) return i;
  return -1;
}
static inline void pressed_map_set(uint8_t kc, uint32_t lvkey) {
  int s = pressed_slot_for(kc);
  if (s < 0) {
    for (int i=0;i<6;++i) if (pressed_map[i].kc == 0) { pressed_map[i] = {kc, lvkey}; return; }
  } else {
    pressed_map[s].lvkey = lvkey;
  }
}
static inline uint32_t pressed_map_get(uint8_t kc) {
  int s = pressed_slot_for(kc);
  return (s >= 0) ? pressed_map[s].lvkey : 0;
}
static inline void pressed_map_clear(uint8_t kc) {
  int s = pressed_slot_for(kc);
  if (s >= 0) pressed_map[s] = {0,0};
}
static inline void pressed_map_clear_all() {
  for (int i=0;i<6;++i) pressed_map[i] = {0,0};
}

static void release_all_pressed_now() {
  for (int i=0;i<6;++i) {
    uint8_t kc = prev_keys[i];
    if (!kc) continue;
    uint32_t lvkey = pressed_map_get(kc);
    if (lvkey) key_enqueue(lvkey, false);
  }
  memset(prev_keys, 0, sizeof(prev_keys));
  prev_mod = 0;
  pressed_map_clear_all();
}

// LVGL indev + group
static lv_indev_t* kb_indev = nullptr;
static lv_group_t* kb_group = nullptr;

// LVGL v8 keypad indev read callback
static void kb_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;
  KeyEvt e;
  if (!key_dequeue(&e)) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->key   = 0;
    return;
  }
  data->key   = e.key;
  data->state = e.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// ===================================
// NimBLE Client callbacks
// ===================================
class LocalClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    (void)pClient;
    Serial.println("[BLE] Connected.");
  }
  void onDisconnect(NimBLEClient* pClient, int reason) override {
    (void)pClient;
    Serial.printf("[BLE] Disconnected, reason=%d\n", reason);
    release_all_pressed_now();
  }
};

// =====================
// BleKeyboardHost impl
// =====================
BleKeyboardHost::BleKeyboardHost() : client(nullptr) {}

void BleKeyboardHost::begin() {
  instance = this;

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_N12);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  // --- LVGL keypad indev for v8 ---
  if (!kb_indev) {
    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = kb_read_cb;
    kb_indev = lv_indev_drv_register(&kb_drv);

    kb_group = lv_group_create();
    lv_group_set_default(kb_group);
    lv_indev_set_group(kb_indev, kb_group);
  }
}

void BleKeyboardHost::connect(NimBLERemoteCharacteristic::notify_callback callback, uint32_t duration) {
  std::vector<NimBLEAddress> bonded;
  for (int i = 0; i < NimBLEDevice::getNumBonds(); i++)
    bonded.push_back(NimBLEDevice::getBondedAddress(i));

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(45);

  Serial.println("[BLE] Scanning for HID device...");
  NimBLEScanResults res = scan->getResults(duration, false);
  Serial.printf("[BLE] Found %d devices.\n", res.getCount());

  for (int i = 0; i < res.getCount(); i++) {
    const NimBLEAdvertisedDevice *dev = res.getDevice(i);
    if (!dev) continue;

    bool addrMatch = false;
    for (size_t j = 0; j < bonded.size(); j++) {
      if (dev->getAddress().equals(bonded[j])) { addrMatch = true; break; }
    }

    bool looksLikeKb = dev->isAdvertisingService(UUID_HID_SERVICE);
    if (!addrMatch && !looksLikeKb) {
        Serial.printf("[BLE] Skip %s (%s)\n",
                  dev->getAddress().toString().c_str(),
                  dev->getName().c_str());
        continue;
    }

    Serial.printf("[BLE] Trying %s (%s)\n",
                  dev->getAddress().toString().c_str(),
                  dev->getName().c_str());

    client = NimBLEDevice::createClient();
    if(!client) {
      Serial.println("[BLE] Failed to create client.");
      continue;
    }
    client->setConnectTimeout(500);
    static LocalClientCallbacks s_cb;
    client->setClientCallbacks(&s_cb, false);

    if (client->connect(dev)) {
      Serial.println("[BLE] Connected via scan match");
      subscribeReports(callback);
      break;
    } else {
      Serial.println("[BLE] Connect failed.");
      client = nullptr;
    }
  }
  scan->clearResults();
}

bool BleKeyboardHost::isReady() const {
  return client && client->isConnected() && !inputs.empty();
}

void BleKeyboardHost::pollLogs() {
  while (!logQueue.empty()) {
    Serial.println(logQueue.front());
    logQueue.pop();
  }
}

void BleKeyboardHost::pushLog(NimBLERemoteCharacteristic *c, uint8_t *data, size_t len, bool isNotify) {
  (void)isNotify;

  char buf[256];
  int pos = snprintf(buf, sizeof(buf), "[HID] Report (len=%d, handle=%u): ",
                     (int)len, c ? c->getHandle() : 0);

  if (len == 8) {
    const uint8_t mod = data[0];
    const bool shift = (mod & 0x22);
    const uint8_t* cur = &data[2];

    bool rollover_error = false;
    for (int i=0;i<6;i++) if (cur[i] == 0x01) { rollover_error = true; break; }
    if (!rollover_error) {
      for (int i=0;i<6;i++) {
        uint8_t kc = cur[i];
        if (kc == 0 || kc_in(prev_keys, kc)) continue;
        uint32_t lvkey = hid_to_lv_key(kc, shift);
        if (!lvkey) continue;
        key_enqueue(lvkey, true);
        pressed_map_set(kc, lvkey);
      }

      for (int i=0;i<6;i++) {
        uint8_t kc = prev_keys[i];
        if (kc == 0 || kc_in(cur, kc)) continue;
        uint32_t lvkey = pressed_map_get(kc);
        if (!lvkey) lvkey = hid_to_lv_key(kc, false);
        if (lvkey) key_enqueue(lvkey, false);
        pressed_map_clear(kc);
      }

      prev_mod = mod;
      for (int i=0;i<6;i++) prev_keys[i] = cur[i];
    }

    if (mod) pos += snprintf(buf + pos, sizeof(buf) - pos, " [mod=0x%02X]", mod);
    pos += snprintf(buf + pos, sizeof(buf) - pos, " [ASCII:");
    for (int i = 0; i < 6; i++) {
      uint8_t kc = cur[i];
      if (!kc) continue;
      char ch = hidToAscii(kc, shift);
      if (ch) pos += snprintf(buf + pos, sizeof(buf) - pos, " '%c'", ch);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
  } else if (len == 3) {
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    " [Media/System: %02X %02X %02X]",
                    data[0], data[1], data[2]);
  } else {
    pos += snprintf(buf + pos, sizeof(buf) - pos, " [Unhandled report]");
  }

  if (logQueue.size() > 50) logQueue.pop();
  logQueue.push(String(buf));
}

void BleKeyboardHost::subscribeReports(NimBLERemoteCharacteristic::notify_callback callback) {
  if (!client) return;

  NimBLERemoteService *hid = client->getService(UUID_HID_SERVICE);
  if (!hid) {
    Serial.println("[HID] HID service not found; disconnecting.");
    client->disconnect();
    return;
  }

  inputs.clear();
  const std::vector<NimBLERemoteCharacteristic*> &chars = hid->getCharacteristics(true);

  for (size_t i = 0; i < chars.size(); i++) {
    NimBLERemoteCharacteristic *chr = chars[i];
    if (!chr) continue;
    if (chr->getUUID() != UUID_REPORT) continue;
    if (!chr->canNotify()) continue;

    Serial.printf("[HID] Subscribing Input Report: handle=%u\n", chr->getHandle());
    if (chr->subscribe(true, callback)) {
      inputs.push_back(chr);
    }
  }
  if (inputs.empty()) {
    Serial.println("[HID] No subscribable Input Reports found.");
  } else {
    Serial.printf("[HID] Subscribed to %u input report(s).\n", (unsigned)inputs.size());
  }
}

char BleKeyboardHost::hidToAscii(uint8_t kc, bool shift) {
  if (kc >= 0x04 && kc <= 0x1D) return shift ? (char)toupper('a' + kc - 0x04) : 'a' + kc - 0x04;
  switch (kc) {
    case 0x1E: return shift ? '!' : '1'; case 0x1F: return shift ? '@' : '2';
    case 0x20: return shift ? '#' : '3'; case 0x21: return shift ? '$' : '4';
    case 0x22: return shift ? '%' : '5'; case 0x23: return shift ? '^' : '6';
    case 0x24: return shift ? '&' : '7'; case 0x25: return shift ? '*' : '8';
    case 0x26: return shift ? '(' : '9'; case 0x27: return shift ? ')' : '0';
    case 0x2C: return ' '; case 0x28: return '\n'; case 0x2A: return '\b'; case 0x2B: return '\t';
    case 0x2D: return shift ? '_' : '-'; case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '['; case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\'; case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\''; case 0x35: return shift ? '~' : '`';
    case 0x36: return shift ? '<' : ','; case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default: return 0;
  }
}
