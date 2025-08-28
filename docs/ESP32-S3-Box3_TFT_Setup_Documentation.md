# ESP32-S3-Box3 TFT Display Setup Documentation

## Overview
This document describes the successful TFT display setup for the ESP32-S3-Box3 development board, including critical configuration requirements and troubleshooting findings.

## Hardware Configuration
- **Board**: ESP32-S3-Box-3 (Espressif)
- **Display**: ILI9341-based TFT LCD (320x240 resolution)
- **Interface**: SPI communication
- **Touch**: I2C-based touch capability

## Critical Configuration Requirement: USE_HSPI_PORT

### ⚠️ IMPORTANT: Must Use HSPI Port
The **`USE_HSPI_PORT`** directive in `TFT_Setup.h` is **CRITICAL** for proper operation. This configuration:

1. **Prevents SPI Bus Conflicts**: ESP32-S3-Box-3 has specific pin assignments that require HSPI port usage
2. **Ensures Proper Pin Mapping**: Maps to the correct hardware SPI pins (SCLK=7, MOSI=6, CS=5)
3. **Avoids Pin Reassignment Issues**: Prevents the system from trying to use VSPI pins that may conflict with other functions

### Current Configuration in TFT_Setup.h
```cpp
#define USE_HSPI_PORT  // Line 8 - CRITICAL for ESP32-S3-Box-3
```

## Pin Assignments
Based on ESP-Box-3 BSP hardware configuration:

| Function | Pin | Description |
|----------|-----|-------------|
| **SCLK** | 7   | BSP_LCD_PCLK (SPI Clock) |
| **MOSI** | 6   | BSP_LCD_DATA0 (SPI Data) |
| **CS**   | 5   | BSP_LCD_CS (Chip Select) |
| **DC**   | 4   | BSP_LCD_DC (Data/Command) |
| **RST**  | -1  | Reset handled manually (ESP-Box-3 inverted logic) |
| **BL**   | 47  | BSP_LCD_BACKLIGHT |
| **MISO** | -1  | Not used for display |

## Display Specifications
- **Driver**: ILI9341 (confirmed working)
- **Resolution**: 320x240 pixels
- **Color Format**: 16-bit color support
- **Font Support**: Multiple font sizes (8px to 75px)
- **Smooth Font**: Enabled for better text rendering

## Memory Usage Analysis
From the successful boot logs:

### Internal Memory
- **Total**: 287.0 KB
- **Free**: 249.4 KB
- **Allocated**: 33.3 KB
- **Largest Block**: 192.0 KB

### SPIRAM
- **Total**: 16 MB
- **Free**: 16.38 MB
- **Allocated**: 0.1 KB
- **Largest Block**: 16.13 MB

## Successful Initialization Sequence
The logs show the following successful initialization steps:

1. **ESP32-S3-Box3 Starting** ✓
2. **TFT Display Initialization** ✓
   - SPI pins configured correctly
   - ESP-Box-3 inverted reset sequence handled
3. **LVGL Initialization** ✓
4. **Setup Complete** ✓

## Key Success Factors

### 1. Correct SPI Port Selection
- **USE_HSPI_PORT** must be defined
- Avoids conflicts with VSPI or other SPI configurations

### 2. Proper Pin Mapping
- Pins 6, 7, 5, 4, 47 are correctly mapped
- Follows ESP-Box-3 BSP specifications

### 3. Reset Logic Handling
- ESP-Box-3 uses inverted reset logic
- Reset pin (-1) indicates manual handling in code

### 4. Backlight Control
- Pin 47 controls backlight
- Successfully turned on during initialization

## Troubleshooting Notes

### Common Issues Avoided
1. **SPI Bus Conflicts**: Resolved by using HSPI port
2. **Pin Reassignment**: Proper pin mapping prevents conflicts
3. **Memory Issues**: Adequate memory allocation for TFT and LVGL

### Verification Commands
The system provides comprehensive memory and GPIO information:
- Memory allocation status
- GPIO bus assignments
- SPI bus configurations

## File Structure
```
include/
└── TFT_Setup.h          # Main configuration file
```

## Dependencies
- ESP32 Arduino Core v3.3.0
- ESP-IDF v5.5-1
- TFT_eSPI library
- LVGL library

## Conclusion
The ESP32-S3-Box-3 TFT display setup is working correctly with the current configuration. The **`USE_HSPI_PORT`** directive is essential and must remain enabled. The system successfully initializes the display, configures SPI communication, and provides a stable foundation for LVGL-based user interfaces.

## Recommendations
1. **Never remove** `#define USE_HSPI_PORT` from TFT_Setup.h
2. **Maintain** current pin assignments as they follow BSP specifications
3. **Monitor** memory usage during development
4. **Use** the provided GPIO and memory information for debugging

---
*Document generated based on successful ESP32-S3-Box-3 TFT display initialization logs*
