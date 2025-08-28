#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <vector>
#include <algorithm>
#include <utility>

#include "ui/ui.h"           // SquareLine export (ui_init)
#include "BLE/BleKeyboardHost.h"
#include "esp_heap_caps.h"
#include "TFT_eSPI.h"
#include "SPI.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

TFT_eSPI tft = TFT_eSPI();

// LVGL Display Buffers - Double buffering for smooth graphics
// Buffer size: 320 pixels wide × 40 lines high × 2 bytes per pixel = 25,600 bytes
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[320 * 40];  // Primary buffer
static lv_color_t buf2[320 * 40];  // Secondary buffer

// LVGL Display Driver
static lv_disp_drv_t disp_drv;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

static void dumpHeap(const char* tag);
void initTFTDisplay();
void initLVGL();
void initUI();

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
    
    // Push the color data to the TFT display
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
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
    digitalWrite(48, HIGH);   // Reset pin is active LOW for ESP-Box-3
    delay(100);
    digitalWrite(48, LOW);    // Pull reset LOW to reset the display
    delay(100);
    Serial.println("Turn on backlight");
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Backlight turned on");
}

// ============================================================================
// TFT DISPLAY INITIALIZATION
// ============================================================================

void initTFTDisplay()
{
    Serial.println("Initializing TFT display...");
        
    // Initialize SPI for TFT
    //SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
    
    // Initialize TFT display
    Serial.println("Calling tft.begin()...");
    tft.begin();
    tft.setRotation(4); // Landscape orientation
    
    // Test basic TFT functionality
    Serial.println("Testing TFT with colors...");
    tft.fillScreen(TFT_RED);
    Serial.println("Red screen drawn");
    delay(1000);
    tft.fillScreen(TFT_GREEN);
    Serial.println("Green screen drawn");
    delay(1000);
    tft.fillScreen(TFT_BLUE);
    Serial.println("Blue screen drawn");
    delay(1000);
    tft.fillScreen(TFT_BLACK);
    Serial.println("Black screen drawn");

    Serial.println("TFT display initialized successfully");
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
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 320 * 40);
    
    // Initialize and configure the display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;           // Display width
    disp_drv.ver_res = 240;           // Display height
    disp_drv.flush_cb = my_disp_flush; // Our callback function
    disp_drv.draw_buf = &draw_buf;     // Use our buffers
    lv_disp_drv_register(&disp_drv);   // Register the driver
    
    Serial.println("LVGL initialized successfully");
}

// ============================================================================
// USER INTERFACE INITIALIZATION
// ============================================================================

void initUI()
{
    Serial.println("Initializing User Interface...");
    
    // Initialize SquareLine Studio generated UI
    ui_init();
    
    // Load the splash screen
    lv_scr_load(ui_Splash);
    
    Serial.println("User Interface initialized successfully");
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
    initTFTDisplay();
    
    // Initialize LVGL graphics library
    initLVGL();
    
    // Monitor memory before UI initialization
    dumpHeap("Before UI init");

    // Initialize user interface
    initUI();

    // Create a simple rectangle at [0,0] to [100,100]
    lv_obj_t * rect = lv_obj_create(lv_scr_act());
    lv_obj_set_size(rect, 100, 100);  // Width: 100, Height: 100
    lv_obj_set_pos(rect, 0, 0);       // Position at [0,0]
    
    // Style the rectangle
    lv_obj_set_style_bg_color(rect, lv_color_hex(0xFF0000), 0);  // Red background
    lv_obj_set_style_border_width(rect, 2, 0);                    // 2px border
    lv_obj_set_style_border_color(rect, lv_color_hex(0xFFFFFF), 0); // White border
    lv_obj_set_style_radius(rect, 0, 0);         
    
    Serial.println("Setup() completed successfully");
}

// ============================================================================
// MAIN LOOP FUNCTION
// ============================================================================

void loop()
{
    // Handle LVGL tasks (drawing, animations, events)
    lv_timer_handler();
    
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
