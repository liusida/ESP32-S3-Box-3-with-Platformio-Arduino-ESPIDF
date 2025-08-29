#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <vector>
#include <algorithm>
#include <utility>

#include "ui/ui.h"           // SquareLine export (ui_init)
#include "esp_heap_caps.h"

#include "TFT_eSPI.h"
#include "GT911.h"

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Touch pin definitions for ESP32-S3-Box-3
#define TOUCH_INT_PIN      3   // BSP_LCD_TOUCH_INT
#define I2C_SDA_PIN        8   // BSP_I2C_SDA  
#define I2C_SCL_PIN        18  // BSP_I2C_SCL

// Touch configuration
#define TOUCH_I2C_ADDR     0x5D  // GT911 default address
#define TOUCH_I2C_FREQ     100000  // 100kHz for compatibility
 
// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

TFT_eSPI tft = TFT_eSPI();
GT911 gt911 = GT911();

// LVGL Display Buffers - Double buffering for smooth graphics
// Buffer size: 320 pixels wide × 40 lines high × 2 bytes per pixel = 25,600 bytes
#define BUF_ROWS 40
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[320 * BUF_ROWS];  // Primary buffer
static lv_color_t buf2[320 * BUF_ROWS];  // Secondary buffer

// LVGL Display Driver
static lv_disp_drv_t disp_drv;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

static void dumpHeap(const char* tag);
void initLVGL();
void initTouch();
void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);

// ============================================================================
// LVGL DISPLAY FLUSH CALLBACK
// ============================================================================

/**
 * This function is called by LVGL when a display buffer is ready to be sent to the TFT.
 * 
 * How it works:
 * 1. LVGL draws content into buf1 or buf2 in RAM
 * 2. When a buffer is full, LVGL calls this function
 * 3. We copy the buffer content to the TFT display via SPI
 * 4. LVGL continues drawing the next frame while we update the display
 * 
 * This enables smooth, flicker-free graphics!
 */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Set the display window to the area we need to update
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    
    // Push pixels (not swapped colors) to the TFT display
    tft.pushPixels((uint16_t *)color_p,
        (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1));
    tft.endWrite();

    // Tell LVGL we're done flushing this area
    lv_disp_flush_ready(disp);
}

// ============================================================================
// MEMORY MONITORING
// ============================================================================

static void dumpHeap(const char* tag)
{
    auto pr = [](const char* name, uint32_t caps){
        Serial.printf("[MEM] %-10s free: %8u, largest: %8u\n",
            name,
            heap_caps_get_free_size(caps),
            heap_caps_get_largest_free_block(caps));
    };
    
    Serial.printf("[MEM] ---- %s ----\n", tag);
    pr("DEFAULT",  MALLOC_CAP_DEFAULT);
    pr("INTERNAL", MALLOC_CAP_INTERNAL);
    pr("DMA",      MALLOC_CAP_DMA);
#if CONFIG_SPIRAM
    pr("SPIRAM",   MALLOC_CAP_SPIRAM);
#endif
    Serial.printf("[MEM] ESP.getFreeHeap(): %u\n", ESP.getFreeHeap());
}

// ============================================================================
// Manual Reset/Backlight Control
// ============================================================================

void manualResetAndBacklight()
{
    // Display current pin configuration
    Serial.printf("SPI Pins: SCLK=%d, MISO=%d, MOSI=%d, CS=%d\n", 
        TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    Serial.printf("TFT Pins: DC=%d, RST=%d, BL=%d\n", 
            TFT_DC, TFT_RST, TFT_BL);

    // ESP-Box-3: Handle inverted reset logic manually
    Serial.println("Handling ESP-Box-3 inverted reset sequence...");
    pinMode(48, OUTPUT);  // Manual reset pin (ESP-Box-3 BSP)
    digitalWrite(48, HIGH);
    delay(100);
    digitalWrite(48, LOW);
    delay(100);
    Serial.println("Turn on backlight");
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Backlight turned on");
}

// ============================================================================
// LVGL INITIALIZATION
// ============================================================================

void initLVGL()
{
    Serial.println("Initializing LVGL...");
    
    // Initialize LVGL core
    lv_init();
    
    // Initialize display buffers for double buffering
    // This prevents flickering and enables smooth animations
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 320 * BUF_ROWS);
    
    // Initialize and configure the display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;           // Display width
    disp_drv.ver_res = 240;           // Display height
    disp_drv.flush_cb = my_disp_flush; // Our callback function
    disp_drv.draw_buf = &draw_buf;     // Use our buffers
    lv_disp_t * disp = lv_disp_drv_register(&disp_drv);   // Register the driver
    
    // Create input device for touch
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    indev_drv.disp = disp;
    lv_indev_drv_register(&indev_drv);
    
    Serial.println("LVGL initialized successfully");
}

// ============================================================================
// TOUCH INITIALIZATION
// ============================================================================


void initTouch()
{
    Serial.println("Initializing touch controller...");
    
    // Initialize GT911 touch controller
    if (gt911.begin(TS_IRQ)) {
        Serial.println("✅ GT911 touch initialized successfully!");
        
        // Print device info
        GTInfo* info = gt911.readInfo();
        if (info) {
            Serial.printf("Product: %s, FW: 0x%04X, Resolution: %dx%d\n", 
                         info->productId, info->fwId, info->xResolution, info->yResolution);
        }
    } else {
        Serial.println("❌ GT911 initialization failed!");
        delay(10000);
        return;
    }
}

// ============================================================================
// LVGL TOUCH READ CALLBACK
// ============================================================================

void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    // Use the standard polling approach as intended by the GT911 library
    uint8_t touches = gt911.touched(GT911_MODE_INTERRUPT);
    
    if (touches > 0) {
        Serial.println("Touch detected");
        // Get touch points
        GTPoint* tp = gt911.getPoints();
        
        // Use first touch point
        uint16_t x = tp[0].x;
        uint16_t y = tp[0].y;

        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        
        // Debug output to verify coordinate mapping
        // Serial.printf("Touch: (%d,%d) -> Display: (%d,%d)\n", x, y, transformed_x, transformed_y);
        Serial.printf("Touch: (%d,%d)\n", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ============================================================================
// MAIN SETUP FUNCTION
// ============================================================================

void setup()
{
    // Initialize serial communication
    Serial.begin(115200);
    Serial.println("=== ESP32-S3-Box3 Starting ===");
    
    // Monitor memory usage
    dumpHeap("After boot");

    manualResetAndBacklight();

    // Initialize TFT display
    tft.begin();
    tft.setRotation(3); // Landscape orientation

    
    // Initialize LVGL graphics library
    initLVGL();
    
    // Monitor memory before UI initialization
    dumpHeap("Before UI init");

    // Initialize SquareLine Studio generated UI
    ui_init();
    
    // Load the splash screen
    lv_scr_load(ui_WIFI_Settings);

    // Initialize touch controller
    initTouch();
    
    Serial.println("Setup() completed successfully");
}

// ============================================================================
// MAIN LOOP FUNCTION
// ============================================================================

void loop()
{
    // Handle LVGL tasks (drawing, animations, events)
    lv_timer_handler();
    
    // No need to check touchDetected anymore - LVGL handles touch via callback
    
    // Increment LVGL tick for timing
    lv_tick_inc(5);
    
    // Print debug info every 10 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 10000) {
        Serial.printf("Loop running, free heap: %u bytes\n", ESP.getFreeHeap());
        lastPrint = millis();
    }
    
    // Small delay to prevent watchdog issues
    delay(5);
}
