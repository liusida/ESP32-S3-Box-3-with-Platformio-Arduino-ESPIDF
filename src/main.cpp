 #include <Arduino.h>
 #include <NimBLEDevice.h>

 const NimBLEUUID UUID_HID_SERVICE((uint16_t)0x1812);
 const NimBLEUUID UUID_REPORT((uint16_t)0x2A4D);
 const uint16_t APPEARANCE_KEYBOARD((uint16_t)0x03C1);
 
 static const NimBLEAdvertisedDevice* advDevice;
 static bool                          doConnect  = false;
 static bool                          isConnected = false;
 static uint32_t                      scanTimeMs = 1000; /** scan time in milliseconds, 0 = scan forever */
 
 /**  None of these are required as they will be handled by the library with defaults. **
  **                       Remove as you see fit for your needs                        */
 class ClientCallbacks : public NimBLEClientCallbacks {
     void onConnect(NimBLEClient* pClient) override { Serial.printf("Connected\n"); }
 
     void onDisconnect(NimBLEClient* pClient, int reason) override {
        //  Serial.printf("%s Disconnected, reason = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
        //  NimBLEDevice::getScan()->start(scanTimeMs, false, true);
        isConnected = false;
     }
 
     /********************* Security handled here *********************/
     void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
         Serial.printf("Server Passkey Entry\n");
         /**
          * This should prompt the user to enter the passkey displayed
          * on the peer device.
          */
         NimBLEDevice::injectPassKey(connInfo, 123456);
     }
 
     void onConfirmPasskey(NimBLEConnInfo& connInfo, uint32_t pass_key) override {
         Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
         /** Inject false if passkeys don't match. */
         NimBLEDevice::injectConfirmPasskey(connInfo, true);
     }
 
     /** Pairing process complete, we can check the results in connInfo */
     void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
         if (!connInfo.isEncrypted()) {
             Serial.printf("Encrypt connection failed - disconnecting\n");
             /** Find the client with the connection handle provided in connInfo */
             NimBLEDevice::getClientByHandle(connInfo.getConnHandle())->disconnect();
             return;
         }
     }
 } clientCallbacks;
 
 /** Define a class to handle the callbacks when scan events are received */
 class ScanCallbacks : public NimBLEScanCallbacks {
     void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
         Serial.printf("Advertised Device found: %s\n", advertisedDevice->toString().c_str());
         if (advertisedDevice->isAdvertisingService(UUID_HID_SERVICE) || 
         (advertisedDevice->haveAppearance() && advertisedDevice->getAppearance() == APPEARANCE_KEYBOARD)) {
             Serial.printf("Found Our Service\n");
             /** stop scan before connecting */
             NimBLEDevice::getScan()->stop();
             /** Save the device reference in a global for the client to use*/
             advDevice = advertisedDevice;
             /** Ready to connect now */
             doConnect = true;
         }
     }
 
     /** Callback to process the results of the completed scan or restart it */
     void onScanEnd(const NimBLEScanResults& results, int reason) override {
      if (!doConnect) {
         Serial.printf("Scan Ended, reason: %d, device count: %d\n", reason, results.getCount());
        //  delay(30000);
        //  NimBLEDevice::getScan()->start(scanTimeMs, false, true);
      }
     }
 } scanCallbacks;
 
 /** Notification / Indication receiving handler callback */
 void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  Serial.printf("=== NOTIFICATION CALLBACK EXECUTED ===\n");
  std::string str  = (isNotify == true) ? "Notification" : "Indication";
     str             += " from ";
     str             += pRemoteCharacteristic->getClient()->getPeerAddress().toString();
     str             += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
     str             += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
     str             += ", Handle = " + std::to_string(pRemoteCharacteristic->getHandle());
     str             += ", Value = [";
     for (size_t i = 0; i < length; i++) {
         if (i > 0) str += ", ";
         char hexStr[8];
        sprintf(hexStr, "0x%02X", pData[i]);
        str += hexStr;
     }
     str += "]";
     Serial.printf("%s\n", str.c_str());
 }
 
 /** Handles the provisioning of clients and connects / interfaces with the server */
 bool connectToServer() {
     NimBLEClient* pClient = nullptr;
 
     /** Check if we have a client we should reuse first **/
     if (NimBLEDevice::getCreatedClientCount()) {
         /**
          *  Special case when we already know this device, we send false as the
          *  second argument in connect() to prevent refreshing the service database.
          *  This saves considerable time and power.
          */
         pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
         if (pClient) {
             if (!pClient->connect(advDevice, false)) {
                 Serial.printf("Reconnect failed\n");
                 return false;
             }
             Serial.printf("Reconnected client\n");
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
             Serial.printf("Max clients reached - no more connections available\n");
             return false;
         }
 
         pClient = NimBLEDevice::createClient();
 
         Serial.printf("New client created\n");
 
         pClient->setClientCallbacks(&clientCallbacks, false);
         /**
          *  Set initial connection parameters:
          *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
          *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
          *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 150 * 10ms = 1500ms timeout
          */
         pClient->setConnectionParams(12, 12, 0, 150);
 
         /** Set how long we are willing to wait for the connection to complete (milliseconds), default is 30000. */
         pClient->setConnectTimeout(5 * 1000);
 
         if (!pClient->connect(advDevice)) {
             /** Created a client but failed to connect, don't need to keep it as it has no data */
             NimBLEDevice::deleteClient(pClient);
             Serial.printf("Failed to connect, deleted client\n");
             return false;
         }
     }
 
     if (!pClient->isConnected()) {
         if (!pClient->connect(advDevice)) {
             Serial.printf("Failed to connect\n");
             return false;
         }
     }
 
     Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());
     
     pClient->secureConnection(); // This fixes write error 261 (insufficient auth)! or in ESP-IDF stack, call `ble_gap_security_initiate(uint16_t conn_handle);`
 
     /** Now we can read/write/subscribe the characteristics of the services we are interested in */
     NimBLERemoteService*        pSvc = nullptr;
     NimBLERemoteCharacteristic* pChr = nullptr;
     NimBLERemoteDescriptor*     pDsc = nullptr;
 
     pSvc = pClient->getService(UUID_HID_SERVICE);
     if (pSvc) {
         // Get ALL characteristics with UUID_REPORT (there might be multiple for different report types)
         const std::vector<NimBLERemoteCharacteristic*> characteristics = pSvc->getCharacteristics(true);
         if (characteristics.size() > 0) {
             Serial.printf("Found %d characteristics with UUID_REPORT\n", characteristics.size());
             
             for (auto chr : characteristics) {
                 Serial.printf("Characteristic handle: %d, canNotify: %s, canIndicate: %s\n", 
                             chr->getHandle(), 
                             chr->canNotify() ? "true" : "false",
                             chr->canIndicate() ? "true" : "false");
                 
                 if (chr->canNotify()) {
                     if (!chr->subscribe(true, notifyCB)) {
                         Serial.printf("Failed to subscribe to notifications on handle %d\n", chr->getHandle());
                         pClient->disconnect();
                         return false;
                     }
                     Serial.printf("Subscribed to notifications on handle %d\n", chr->getHandle());
                 } else if (chr->canIndicate()) {
                     if (!chr->subscribe(false, notifyCB)) {
                         Serial.printf("Failed to subscribe to indications on handle %d\n", chr->getHandle());
                         pClient->disconnect();
                         return false;
                     }
                     Serial.printf("Subscribed to indications on handle %d\n", chr->getHandle());
                 }
             }
         } else {
             Serial.printf("No characteristics found with UUID_REPORT\n");
             pClient->disconnect();
             return false;
         }
     } else {
         Serial.printf("UUID_HID_SERVICE service not found.\n");
     }
 
 
     Serial.printf("Done with this device!\n");
     return true;
 }
 
 void setup() {
     Serial.begin(115200);
     Serial.printf("Starting NimBLE Client\n");
 
     /** Initialize NimBLE and set the device name */
     NimBLEDevice::init("NimBLE-Client");
 
     /**
      * Set the IO capabilities of the device, each option will trigger a different pairing method.
      *  BLE_HS_IO_KEYBOARD_ONLY   - Passkey pairing
      *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
      *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
      */
     // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
     // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison
 
     /**
      * 2 different ways to set security - both calls achieve the same result.
      *  no bonding, no man in the middle protection, BLE secure connections.
      *  These are the default values, only shown here for demonstration.
      */
    //  NimBLEDevice::setSecurityAuth(true, true, true);
     // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
 
     /** Optional: set the transmit power */
     NimBLEDevice::setPower(3); /** 3dbm */
     NimBLEScan* pScan = NimBLEDevice::getScan();
 
     /** Set the callbacks to call when scan events occur, no duplicates */
     pScan->setScanCallbacks(&scanCallbacks, false);
 
     /** Set scan interval (how often) and window (how long) in milliseconds */
     pScan->setInterval(100);
     pScan->setWindow(100);
 
     /**
      * Active scan will gather scan response data from advertisers
      *  but will use more energy from both devices
      */
     pScan->setActiveScan(true);
 
     /** Start scanning for advertisers */
     pScan->start(scanTimeMs);
     Serial.printf("Scanning for peripherals\n");
 }
 
 void loop() {
     /** Loop here until we find a device we want to connect to */
     delay(10);
 
     if (doConnect) {
         doConnect = false;
         /** Found a device we want to connect to, do it now */
         if (connectToServer()) {
             isConnected = true;
             Serial.printf("Success! we should now be getting notifications!\n");
         } else {
             Serial.printf("Failed to connect, starting scan\n");
         }
        //  delay(20000);
        //  NimBLEDevice::getScan()->start(scanTimeMs, false, true);
     }
     if (!isConnected) {
      if (!NimBLEDevice::getScan()->isScanning()) {
        Serial.printf("No connection, scanning for peripherals\n");
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
      }
     }
 }