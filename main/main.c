#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "gu_camera.h"
#include "http_client.h"
#include "wifi.h"
#include "gu_key_touch.h"

#define WIFI_SSID "APLD-417-2.4G"
#define WIFI_PASS "1223334444aplid"

#define TAG "main"

#define LED_G_IO    4
#define LED_RGB_IO_SEL  (1ULL<<LED_G_IO) 

static void Led_Gpio_Config(void)
{
    //led端口初始化,端口位推挽输出模式
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.pin_bit_mask = LED_RGB_IO_SEL;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;

    gpio_config(&io_conf); 
}

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
    
    Led_Gpio_Config();
    

	init_camera();
	key_touch_init();
	wifi_start(&wifi);
}

