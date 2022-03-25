#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "driver/touch_pad.h"
#include "soc/rtc_periph.h"
#include "soc/sens_periph.h"
#include "driver/gpio.h"

#include "gu_key_touch.h"
#include "http_client.h"

static const char *TAG = "Touch pad";

#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (80)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)



static bool s_pad_activated[TOUCH_PAD_MAX];
static uint32_t s_pad_init_val[TOUCH_PAD_MAX];

/*
  Read values sensed at all available touch pads.
  Use 2 / 3 of read value as the threshold
  to trigger interrupt when the pad is touched.
  Note: this routine demonstrates a simple way
  to configure activation threshold for the touch pads.
  Do not touch any pads when this routine
  is running (on application start).
 */
static void tp_example_set_thresholds(void)
{
    uint16_t touch_value;
    for (int i = 0; i < TOUCH_PAD_MAX; i++) 
    {
        //read filtered value
        touch_pad_read_filtered(i, &touch_value);
        s_pad_init_val[i] = touch_value;
        ESP_LOGI(TAG, "test init: touch pad [%d] val is %d", i, touch_value);
        //set interrupt threshold.
        ESP_ERROR_CHECK(touch_pad_set_thresh(i, touch_value * 2 / 3));

    }
}

// 检测触摸中断任务
static void tp_example_read_task(void *pvParameter)
{
    static int show_message;

    while (1) 
    {
        //interrupt mode, enable touch interrupt
        touch_pad_intr_enable();
        for (int i = 0; i < TOUCH_PAD_MAX; i++) {
            if (s_pad_activated[i] == true) {
                ESP_LOGI(TAG, "T%d activated!", i);
                // Wait a while for the pad being released
                vTaskDelay(500 / portTICK_PERIOD_MS);
                // Clear information on pad activation
                s_pad_activated[i] = false;
                // Reset the counter triggering a message
                // that application is running 
                do_start_http_client();
                show_message = 1;
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);

        // If no pad is touched, every couple of seconds, show a message
        // that application is running
        if (show_message++ % 500 == 0) {
            ESP_LOGI(TAG, "Waiting for any pad being touched...");
        }
    }
}

// 触摸中断处理函数。触摸过的端口保存在s_pad_activated数组中
static void tp_example_rtc_intr(void *arg)
{
    uint32_t pad_intr = touch_pad_get_status();
    //clear interrupt
    touch_pad_clear_status();

    if ((pad_intr >> 6) & 0x01) 
    {
        s_pad_activated[6] = true;  
    }
}

/*
 * Before reading touch pad, we need to initialize the RTC IO.
 */
// static void tp_example_touch_pad_init(void)
// {
//     for (int i = 0; i < TOUCH_PAD_MAX; i++) {
//         //init RTC IO and mode for touch pad.
//         touch_pad_config(i, TOUCH_THRESH_NO_USE);
//     }
// }

void key_touch_init(void)
{
    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing touch pad");
    touch_pad_init();
    // 如果使用中断触发模式，应将触摸传感器FSM模式设置为“ TOUCH_FSM_MODE_TIMER”
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    // 设定充放电参考电压：高参考电压，低参考电压，高参考电压衰减
    // the high reference valtage will be 2.7V - 1V = 1.7V, The low reference voltage will be 0.5V.
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    // 配置触摸端口
    // tp_example_touch_pad_init();
    touch_pad_config(6,TOUCH_THRESH_NO_USE);
    // 初始化并启动软件滤波器
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
    // 设定中断限值，此时不要触摸，2/3的读取值做为限值
    tp_example_set_thresholds();
    // 注册触摸中断ISR
    touch_pad_isr_register(tp_example_rtc_intr, NULL);
    // 开启一个任务处理电容触摸
    xTaskCreate(&tp_example_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
}
