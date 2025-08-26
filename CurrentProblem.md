## Not supporting ESP32-S3-Box-3 perfectly

The display is dark, GPIO47 has confliction, and I2C has errors.

### solution

refer to https://github.com/espressif/esp-box/#master, because the `factory demo` is working.

## NetworkClientSecure linking issue

During linking:

/Users/star/.platformio/packages/framework-arduinoespressif32/libraries/NetworkClientSecure/src/NetworkClientSecure.cpp:35:(.text._ZN19NetworkClientSecureC2Ev+0x33): undefined reference to `_Z8ssl_initP17sslclient_context'

### solution

add these to sdkconfig.defaults:

CONFIG_MBEDTLS_SSL_CLI_C=y
CONFIG_MBEDTLS_PSK_MODES=y
CONFIG_MBEDTLS_KEY_EXCHANGE_PSK=y
CONFIG_MBEDTLS_KEY_EXCHANGE_ECDHE_PSK=y
CONFIG_MBEDTLS_KEY_EXCHANGE_RSA_PSK=y