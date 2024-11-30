# ESP32C3 DevKit Music Player GUI

This project is a music player daemon (MPD) client for the ESP32C3 DevKit. It is a simple GUI that allows you to control the music player daemon running on your computer. The GUI is built using the [LVGL](https://lvgl.io/) library.

## Features
- Play/Pause
- Next/Previous
- Volume control
- Display song information
<img src="https://github.com/sam-k0/ESP32C3-MPD-GUI/blob/master/img/img.jpg" alt="Screenshot" width="300"/>

## Setup

You need to create a `my_secrets.h` file in the `main` directory with the following content:

```cpp
#define WIFI_SSID "AmOGuS"
#define WIFI_PASSWORD "fortnitebattlepass123"
#define MPD_HOST "192.168.178.1"
#define MPD_PORT 6600
```

## Requirements
- espressif/esp32_c3_lcdkit: "1.0.*"
- idf: ">=4.1.0"