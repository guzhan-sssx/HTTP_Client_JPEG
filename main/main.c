#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "gu_camera.h"
#include "http_client.h"
#include "wifi.h"

#define WIFI_SSID "APLD-417-2.4G"
#define WIFI_PASS "1223334444aplid"

#define TAG "main"

void on_wifi_callback(wifi_event_type_t event);


static wifi_t wifi = 
{
    .ssid = WIFI_SSID,
    .password = WIFI_PASS,
    .mode = WIFI_MODE_STA,
    .netif = NULL,
    .callback = on_wifi_callback
};




void app_main(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());
	init_camera();
	wifi_start(&wifi);
}

