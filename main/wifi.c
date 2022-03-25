/*
 * wifi.c
 *
 *  Created on: 2021年7月5日
 *      Author: jenson
 */
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include <string.h>

#include "wifi.h"

#define WIFI_CONNECT_MAXIMUM_RETRY 5
#define TAG "wifi"

// WiFi事件组句柄
static EventGroupHandle_t wifi_event_group;

const int WIFI_CONNECTED_BIT = BIT0; // WIFI连接事件组位
const int WIFI_DISCONNECTED_BIT = BIT1; // WIFI断开连接事件组位
// 连接尝试次数
static int s_retry_num = 0;

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
void wifi_event_task(void* params);

void wifi_start(wifi_t* wifi){
    // WiFi事件组
    wifi_event_group = xEventGroupCreate();

    // 初始化IP协议层
    ESP_ERROR_CHECK(esp_netif_init());
    // 启动事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 创建Station模式的IP协议层
    wifi->netif  = esp_netif_create_default_wifi_sta();

     // 获取WiFi默认初始化配置
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    // 注册WiFi事件
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    // 注册WiFi连接事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    // 注册IP事件，Station工作模式下获取IPV4事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    // 注册IP事件，Station工作模式下获取IPV6事件
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &wifi_event_handler, NULL));



    wifi_config_t wifi_cfg;
    // 将wifi_config_t对象所有字段清零
    memset(&wifi_cfg,0,sizeof(wifi_config_t));

    // 复制SSID到配置对象中，SSID长度为33个字符
    // 如果SSID过长，将截断，将导致无法连接上WiFi网络
    size_t ssid_len = strlen(wifi->ssid);
    if(ssid_len > 33){
        ssid_len = 33;
    }
    memcpy(wifi_cfg.sta.ssid,wifi->ssid,ssid_len);

    // 复制密码
    size_t pass_len = strlen(wifi->password);
    if(pass_len > 64){
       pass_len = 64;
    }
    memcpy(wifi_cfg.sta.password,wifi->password,pass_len);

    ESP_LOGI(TAG,"wifi ssid:%s,passwrod:%s\n",wifi_cfg.sta.ssid,wifi_cfg.sta.password);
    // 设置认证模式
    if(pass_len > 0){
    	wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }else{
    	wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    // 保护管理帧的配置
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    // 设置WiFi的工作模式为Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    // 应用WiFi配置
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA,&wifi_cfg));
    // 启动WiFi连接
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,"connecting to %s...",wifi->ssid);

    // 创建WiFi事件处理任务
    xTaskCreate(wifi_event_task,"wifi_event_task",2048,(void*)wifi,3,NULL);
}

void wifi_stop(wifi_t* wifi){
    esp_wifi_stop();
}

/**
 * @brief WiFi事件处理函数
 * */
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data){
	// WiFi在Station模式启动事件
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        ESP_LOGI(TAG,"wifi started,start to connect...");
        esp_wifi_connect();
    }
    // WiFi在Station模式下断开连接
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if (s_retry_num < WIFI_CONNECT_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
        	// 触发WiFi断开事件
            xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    }

    // WiFi连接后，获取IP事件
    if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ESP_LOGI(TAG,"wifi has got ip.");
        // 触发事件WIFI_CONNECTED_BIT组
        xEventGroupSetBits(wifi_event_group,WIFI_CONNECTED_BIT);
    }
}

void wifi_event_task(void* params){
    EventBits_t event_bits;
    wifi_t* wifi = (wifi_t*)params;
    for(;;){
        ESP_LOGI(TAG,"waiting wifi events...");
        // 等待事件
        event_bits = xEventGroupWaitBits(wifi_event_group,WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,pdTRUE,pdFALSE,portMAX_DELAY);
        ESP_LOGI(TAG,"wifi event comming...");

        // WiFi连接并分配IP事件
        if((event_bits & WIFI_CONNECTED_BIT) == WIFI_CONNECTED_BIT){
            ESP_LOGI(TAG,"connected to wifi network");
            tcpip_adapter_ip_info_t ip_info;
            // 查询、转换IP地址、网关、子网掩码
            ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA,&ip_info));
            printf("ip address:%s\n",ip4addr_ntoa(&ip_info.ip));
            printf("subnet mask:%s\n",ip4addr_ntoa(&ip_info.netmask));
            printf("gateway:%s\n",ip4addr_ntoa(&ip_info.gw));

            // 自定义WiFi事件回调函数
            if(wifi->callback != NULL){
                ESP_LOGI(TAG,"connected,call callback function");
                wifi->callback(EVENT_GOT_IP);
            }else{
                ESP_LOGI(TAG,"callback is null");
            }
        }

        // WiFi断开连接事件
        if((event_bits & WIFI_DISCONNECTED_BIT) == WIFI_DISCONNECTED_BIT){
            ESP_LOGI(TAG,"waiting for wifi disconnect");
            ESP_LOGI(TAG,"wifi disconected from the wifi network");
            
            // 自定义WiFi事件回调函数
            if(wifi->callback != NULL){
                ESP_LOGI(TAG,"disconnected,call callback function");
                wifi->callback(EVENT_DISCONNECT);
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
}

