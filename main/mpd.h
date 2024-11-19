#pragma once

#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "esp_system.h" //esp_init funtions esp_err_t 
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_event.h" //for wifi event


#define WIFI_SSID "Bagger 244"
#define WIFI_PASSWORD "am0gu$$y69"
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char* WIFI_TAG = "wifi station";
static int s_retry_num = 0;

#ifdef __cplusplus
extern "C" {
#endif

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

void wifi_init_sta(void);

#ifdef __cplusplus
}
#endif
