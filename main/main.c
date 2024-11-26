#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

#include "lv_example_pub.h"
#include "mpd.h"


static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    bsp_display_start(); // Initialize display
    ui_obj_to_encoder_init(); // Initialize encoder
    lv_create_home(&home_layer); // Create home layer
    bsp_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_display_backlight_on();
    wifi_init_sta(); // Init wifi station
}