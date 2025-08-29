#include "ScanCallbacks.h"
#include "BleKeyboardHost.h"
#include "NimBLELog.h"

static const char *LOG_TAG = "ScanCallbacks";

/** Define a class to handle the callbacks when scan events are received */
void ScanCallbacks::onResult(
    const NimBLEAdvertisedDevice *advertisedDevice) {
  NIMBLE_LOGI(LOG_TAG, "Advertised Device found: %s",
              advertisedDevice->toString().c_str());
  if (advertisedDevice->isAdvertisingService(UUID_HID_SERVICE) ||
      (advertisedDevice->haveAppearance() &&
       advertisedDevice->getAppearance() == APPEARANCE_KEYBOARD)) {
    NIMBLE_LOGI(LOG_TAG, "Found Our Service");
    /** stop scan before connecting */
    NimBLEDevice::getScan()->stop();
    /** Save the device reference in a global for the client to use*/
    host->m_advDevice = advertisedDevice;
    /** Ready to connect now */
    host->m_doConnect = true;
  }
}

/** Callback to process the results of the completed scan or restart it */
void ScanCallbacks::onScanEnd(const NimBLEScanResults &results,
                              int reason) {
  if (!host->m_doConnect) {
    NIMBLE_LOGI(LOG_TAG, "Scan Ended, reason: %d, device count: %d", reason,
                results.getCount());
  }
}

void ScanCallbacks::setHost(BleKeyboardHost* host) {
  this->host = host;
}