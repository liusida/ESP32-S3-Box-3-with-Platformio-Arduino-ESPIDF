#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include <utility>

#include "bsp/esp-bsp.h"     // Espressif BSP: display, audio, etc.
#include "ui/ui.h"           // SquareLine export (ui_init)
#include "BLE/BleKeyboardHost.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


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


void setup()
{
    Serial.begin(115200);
    delay(500);
    dumpHeap("After boot");

    // --- BSP display init (handles LVGL + drivers automatically) ---
    bsp_display_start();
    bsp_display_backlight_on();

    dumpHeap("Before UI init");

    // --- SquareLine UI init ---
    ui_init();
    Serial.println("Setup() done.");
}

void loop()
{
    lv_timer_handler();
    lv_tick_inc(5);   // increment LVGL tick
    delay(5);
}
