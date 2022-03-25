/*
 * wifi.h
 *
 *  Created on: 2021年7月5日
 *      Author: jenson
 */


#ifndef __WIFI_H__
#define __WIFI_H__

#include "esp_wifi.h"

/**
 * @brief WiFi事件类型
 * */
typedef enum{
    EVENT_GOT_IP, /*获取IP*/
    EVENT_CONNECTED, /*WiFi连接*/
    EVENT_DISCONNECT /*WiFi断开连接*/
}wifi_event_type_t;

/**
 * @brief WiFi事件回调函数
 * @param event WiFi事件
 * */
typedef void (*on_wifi_event_callback)(wifi_event_type_t event);

/**
 * @brief WiFi设备定义
 * */
typedef struct {
    char* ssid; /*所要连接的SSID*/
    char* password; /*密码*/
    wifi_mode_t mode; /*WiFi工作模式*/
    esp_netif_t* netif; /*WiFi网卡对象*/
    on_wifi_event_callback callback; /*事件回调对象*/
}wifi_t;

/**
 * @brief 启动WiFi连接
 * @param wifi WiFi设备对象
 * */
void wifi_start(wifi_t* wifi);

/**
 * @brief 关闭WiFi连接
 * @param wifi WiFi设备对象
 * */
void wifi_stop(wifi_t* wifi);

#endif
