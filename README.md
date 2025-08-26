# ESP32-S3-Box-3 PlatformIO Project Template

This repository provides a **PlatformIO project template** for the **ESP32-S3-Box-3**, enabling development with **Arduino-style programming** while still having access to **ESP-IDF components** for hardware support (touch, display, etc.).

PlatformIO currently provides board support for the **ESP32-S3-Box**, but not the Box-3, which has key differences. This template bridges that gap and integrates the necessary configuration and components to get you started quickly.

---

## ✨ Features
- Custom `esp32s3box3` board definition for PlatformIO.
- Works with both **Arduino** and **ESP-IDF** development flows.
- Integrated **BSP (Board Support Package)** for ESP32-S3-Box-3.
- Preconfigured **LVGL v8.3.11**, the version supported by SquareLine Studio.
- SquareLine Studio UI export ready (`./src/ui`).
- Customizable ESP-IDF and LVGL configurations.
- SSL certificate placeholder support.

---

## ⚙️ Configuration Details

### `platformio.ini`
Defines main PlatformIO environments and build settings.

### `boards/esp32s3box3.json`
Custom board definition that lets you use `esp32s3box3` instead of `esp32s3box`.

### `components/`
Contains the ESP32-S3-Box-3 BSP, copied from:
[Espressif ESP-BOX BSP](https://github.com/espressif/esp-box/tree/master/components/bsp)  
This removes the need to manually manage drivers like **TFT_eSPI**.

### `LVGL`
- LVGL v8.3.11 is included for UI development.
- Settings customized in [`include/lv_conf.h`](./include/lv_conf.h).
- UI exported from SquareLine Studio lives in [`src/ui`](./src/ui).

### ESP-IDF Integration
- `src/idf_component.yml`: Specifies ESP-IDF components to install.
- `sdkconfig.defaults`: Custom IDF settings (later expanded to `sdkconfig.*` per environment).

### SSL Certificates
- Certificates are stored in [`certs/`](./certs/).
- These are **placeholders** — do not use them in production.
- Generate your own certificates for security.

---

## 🚀 Getting Started

1. Install [PlatformIO](https://platformio.org/) in VSCode.

2. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/esp32s3box3-template.git
   cd esp32s3box3-template
   ```

3. Build and upload your project:

    ```bash
    pio run --target upload
    ```

4. Monitor serial output:

    ```bash
    pio device monitor
    ```

---

## 🔮 Notes

This template exists because PlatformIO doesn’t yet support ESP32-S3-Box-3.

Once official support is added, you may no longer need this setup.

Until then, this serves as a solid starting point for ESP32-S3-Box-3 projects.