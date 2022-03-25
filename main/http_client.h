#ifndef _HTTP_CLIENT_H
#define _HTTP_CLIENT_H

#include "wifi.h"

//函数声明
/**
 * @brief wifi调用回调函数
 * @param 
 * */
void on_wifi_callback(wifi_event_type_t event);

void http_client_task(void* params);

int do_start_http_client();


#endif 
