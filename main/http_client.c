#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "http_client.h"
#include "esp_camera.h"

#define TAG "http_client"

//巴法云上传图片需要的HTTP请求头
// const char*  post_url = "http://images.bemfa.com/upload/v1/upimages.php"; // 默认上传地址
const char*  uid = "f0f615a348144fd49ece3fc2c0213ee9";    //用户私钥，巴法云控制台获取
const char*  topic = "jpegtest1";     //主题名字，可在控制台新建
const char*  wechatMsg = "";          //如果不为空，会推送到微信，可随意修改，修改为自己需要发送的消息
const char*  wecomMsg = "";          //如果不为空，会推送到企业微信，推送到企业微信的消息，可随意修改，修改为自己需要发送的消息
const char*  urlPath = "";           //如果不为空，会生成自定义图片链接，自定义图片上传后返回的图片url，url前一部分为巴法云域名，第二部分：私钥+主题名的md5值，第三部分为设置的图片链接值。


//函数声明
void on_wifi_callback(wifi_event_type_t event);
void http_client_task(void* params);
int do_start_http_client();
esp_err_t on_http_event(esp_http_client_event_t *evt);




esp_http_client_config_t config_with_auth = {
        .url = "http://images.bemfa.com/upload/v1/upimages.php", // 远程主机
		.event_handler = on_http_event, // 事件处理
};



void on_wifi_callback(wifi_event_type_t event)
{
    if(event == EVENT_GOT_IP)
    {
    	do_start_http_client();
    }

    if(event == EVENT_DISCONNECT)
    {

    }
}

//初始化http_client
int do_start_http_client(){

	//分配空间，初始化等 配置http客户端参数，分配需要的资源
	esp_http_client_handle_t client = esp_http_client_init(&config_with_auth);

	if(client == NULL){
		ESP_LOGE(TAG,"cannot create http client object.");
		return ESP_FAIL;
	}
	xTaskCreate(http_client_task, "http_client", 2048,client, 2, NULL);
	return ESP_OK;
}


//事件查询
esp_err_t on_http_event(esp_http_client_event_t *evt){
	switch(evt->event_id){
	case HTTP_EVENT_ERROR: // HTTP出错事件
		ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
	break;
	 case HTTP_EVENT_ON_CONNECTED: // 正在连接事件
	    ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
	break;
	case HTTP_EVENT_HEADER_SENT: // HTTP请求头已经发送事件事件
	    ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
	break;
	case HTTP_EVENT_ON_HEADER: // 接收到响应头事件
		ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
	    printf("%.*s", evt->data_len, (char*)evt->data);
	break;
	case HTTP_EVENT_ON_DATA: // 接收到响应数据事件
	    ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
	    if (!esp_http_client_is_chunked_response(evt->client)) {
	    	printf("%.*s", evt->data_len, (char*)evt->data);
	    }
	 break;
	 case HTTP_EVENT_ON_FINISH: // HTTP请求完成事件
	      ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
	 break;
	 case HTTP_EVENT_DISCONNECTED: // HTTP断开连接事件
	      ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
	 break;
	}

	return ESP_OK;
}


//post 拍照上传函数
void http_client_task(void* params){

	camera_fb_t * fb = NULL;



    fb = esp_camera_fb_get();
	// esp_camera_fb_return(fb);
	if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");  
    }

	esp_http_client_handle_t client = (esp_http_client_handle_t)params;
	// // 表单参数
	// const char *post_data = "field1=value1&field2=value2";
	// // 设置请求路径
	// esp_http_client_set_url(client, "/post");
	// 设置请求方法
	esp_http_client_set_method(client, HTTP_METHOD_POST);
	// 设置表单数据
	//通过post方式请求的数据，传入发送数据的缓存地址与长度，必须在esp_http_client_perform之前调用
	// esp_http_client_set_post_field(client, post_data, strlen(post_data));
  	esp_http_client_set_post_field(client, (const char *)fb->buf, fb->len);//设置http发送的内容和长度
	
	esp_http_client_set_header(client, "Content-Type", "image/jpg"); //设置http头部字段
	esp_http_client_set_header(client, "Authorization", uid);        //设置http头部字段
	esp_http_client_set_header(client, "Authtopic", topic);          //设置http头部字段
	esp_http_client_set_header(client, "wechatmsg", wechatMsg);      //设置http头部字段
	esp_http_client_set_header(client, "wecommsg", wecomMsg);        //设置http头部字段
	esp_http_client_set_header(client, "picpath", urlPath);          //设置http头部字段

	// 执行请求
	esp_err_t err = esp_http_client_perform(client);//发送HTTP请求
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
		                esp_http_client_get_status_code(client),
		                esp_http_client_get_content_length(client));
	} else {
		ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
	}
	// 关闭清空HTTPP客户端
	esp_http_client_cleanup(client);

	esp_camera_fb_return(fb);
	vTaskDelete(NULL);
}
