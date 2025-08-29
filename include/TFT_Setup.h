// #warning "using TFT_Setup.h"
// #include "TFT_eSPI.h"
// #ifndef _TFT_eSPIH_
// #define _TFT_eSPIH_ // to avoid multiple include TFT_eSPI.h after this file, otherwise pins will be redefined
// #endif

// Display configuration for ESP32-S3-Box-3 (ILI9341-based)
// Based on ESP-Box-3 BSP hardware configuration
#define USER_SETUP_ID 252
#define USER_SETUP_INFO "ESP32-S3-BOX-3"

#define M5STACK
#define ILI9341_DRIVER
#define USE_HSPI_PORT

// Pin definitions for ESP32-S3-Box-3 (from BSP)
#define TFT_MISO -1  // Not used for display
#define TFT_MOSI 6   // BSP_LCD_DATA0
#define TFT_SCLK 7   // BSP_LCD_PCLK
#define TFT_CS   5   // BSP_LCD_CS
#define TFT_DC   4   // BSP_LCD_DC
#define TFT_RST  -1  // Reset handled manually in code (ESP-Box-3 inverted logic)
#define TFT_BOX_3_RESET 48
#define TFT_BL  47   // BSP_LCD_BACKLIGHT

// Touch configuration (ESP-Box-3 has touch capability)
#define TOUCH_CS -1  // Touch uses I2C, not SPI CS

// Display resolution (from BSP)
// #define TFT_WIDTH  320
// #define TFT_HEIGHT 240

// Color format (from BSP)
#define LOAD_GLCD  // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2 // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4 // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6 // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7 // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8 // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT
