#include "ClientCallbacks.h"
#include "NimBLELog.h"

static const char *LOG_TAG = "ClientCallbacks";

/**  None of these are required as they will be handled by the library with
 *defaults. **
 **                       Remove as you see fit for your needs */

void ClientCallbacks::onConnect(NimBLEClient *pClient) {
  NIMBLE_LOGI(LOG_TAG, "Connected");
}

void ClientCallbacks::onDisconnect(NimBLEClient *pClient, int reason) {
  //  Serial.printf("%s Disconnected, reason = %d - Starting scan\n",
  //  pClient->getPeerAddress().toString().c_str(), reason);
  //  NimBLEDevice::getScan()->start(scanTimeMs, false, true);
  host->m_isConnected = false;
}

/********************* Security handled here *********************/
void ClientCallbacks::onPassKeyEntry(NimBLEConnInfo &connInfo) {
  NIMBLE_LOGI(LOG_TAG, "Server Passkey Entry");
  /**
   * This should prompt the user to enter the passkey displayed
   * on the peer device.
   */
  NimBLEDevice::injectPassKey(connInfo, 123456);
}

void ClientCallbacks::onConfirmPasskey(NimBLEConnInfo &connInfo,
                                       uint32_t pass_key) {
  NIMBLE_LOGI(LOG_TAG, "The passkey YES/NO number: %" PRIu32, pass_key);
  /** Inject false if passkeys don't match. */
  NimBLEDevice::injectConfirmPasskey(connInfo, true);
}

/** Pairing process complete, we can check the results in connInfo */
void ClientCallbacks::onAuthenticationComplete(
    NimBLEConnInfo &connInfo) {
  if (!connInfo.isEncrypted()) {
    NIMBLE_LOGE(LOG_TAG, "Encrypt connection failed - disconnecting");
    /** Find the client with the connection handle provided in connInfo */
    NimBLEDevice::getClientByHandle(connInfo.getConnHandle())->disconnect();
    return;
  }
}

void ClientCallbacks::setHost(BleKeyboardHost* host) {
  this->host = host;
}