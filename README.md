PACKAGES: 
 - contrib-piohome @ 3.4.4 
 - framework-arduinoespressif32 @ 3.3.0 
 - framework-arduinoespressif32-libs @ 5.5.0+sha.b66b5448e0 
 - framework-espidf @ 3.50500.0 (5.5.0) 
 - tool-cmake @ 3.30.2 
 - tool-dfuutil-arduino @ 1.11.0 
 - tool-esp-rom-elfs @ 2024.10.11 
 - tool-esptoolpy @ 5.0.2 
 - tool-mkfatfs @ 2.0.1 
 - tool-mklittlefs @ 3.2.0 
 - tool-mklittlefs4 @ 4.0.2 
 - tool-mkspiffs @ 2.230.0 (2.30) 
 - tool-ninja @ 1.13.1 
 - tool-scons @ 4.40801.0 (4.8.1) 
 - toolchain-xtensa-esp-elf @ 14.2.0+20241119
Warning! Arduino framework as an ESP-IDF component doesn't handle the `variant` field! The default `esp32` variant will be used.
Reading CMake configuration...
Warning! Flash memory size mismatch detected. Expected 16MB, found 2MB!
Please select a proper value in your `sdkconfig.defaults` or via the `menuconfig` target!
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 45 compatible libraries
Scanning dependencies...
Dependency Graph
|-- NimBLE-Arduino @ 2.3.4
|-- ArduinoJson @ 7.4.2
|-- TFT_eSPI @ 2.5.43
|-- lvgl @ 9.1.0
|-- HTTPClient @ 3.3.0
|-- WiFi @ 3.3.0
|-- NetworkClientSecure @ 3.3.0

# fix

to fix the "idf_tools.py installation failed" problem:
```
export SSL_CERT_FILE="$(
  /Users/star/.platformio/penv/bin/python -c 'import certifi; print(certifi.where())'
)"
export REQUESTS_CA_BUNDLE="$SSL_CERT_FILE"
```
