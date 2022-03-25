
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_camera.h"

#define WIFI_SSID "APLD-417-2.4G"
#define WIFI_PASS "1223334444aplid"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 512

// #define HOST  "192.168.1.121"
// #define USERNAME  "user"
// #define PASSWORD  "challenge"

#define TAG "http_request:main"

// const char*  post_url = "http://images.bemfa.com/upload/v1/upimages.php"; // 默认上传地址
const char*  uid = "f0f615a348144fd49ece3fc2c0213ee9";    //用户私钥，巴法云控制台获取
const char*  topic = "jpegtest1";     //主题名字，可在控制台新建
const char*  wechatMsg = "";          //如果不为空，会推送到微信，可随意修改，修改为自己需要发送的消息
const char*  wecomMsg = "";          //如果不为空，会推送到企业微信，推送到企业微信的消息，可随意修改，修改为自己需要发送的消息
const char*  urlPath = "";           //如果不为空，会生成自定义图片链接，自定义图片上传后返回的图片url，url前一部分为巴法云域名，第二部分：私钥+主题名的md5值，第三部分为设置的图片链接值。


void on_wifi_callback(wifi_event_type_t event);
void http_client_task(void* params);
int do_start_http_client();
esp_err_t on_http_event(esp_http_client_event_t *evt);

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22



// static const char *TAG = "example:take_picture";

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_UXGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 20, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

//初始化相机
static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }else{
        ESP_LOGE(TAG, "Camera Init Successe");
        return err;
    }

    return ESP_OK;
}


static wifi_t wifi = {
    .ssid = WIFI_SSID,
    .password = WIFI_PASS,
    .mode = WIFI_MODE_STA,
    .netif = NULL,
    .callback = on_wifi_callback
};

// 用户数据缓存
// static char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
//这里主要是初始化了 URL 和http的一个事件
// static esp_http_client_config_t config_with_auth = {
//         .host = HOST, // 远程主机
//         .path = "/get", // 请求路径 访问资源的路径
// 		.query = "esp", // 请求参数
// 		.event_handler = on_http_event, // 事件处理
// 		.disable_auto_redirect = true, // 自动跳转
// 		.user_data = local_response_buffer, // 用户自定义数据缓存
// };

static esp_http_client_config_t config_with_auth = {
        .url = "http://images.bemfa.com/upload/v1/upimages.php", // 远程主机
        // .path = "/get", // 请求路径 访问资源的路径
		// .query = "esp", // 请求参数
		.event_handler = on_http_event, // 事件处理
		// .disable_auto_redirect = true, // 自动跳转
		// .user_data = local_response_buffer, // 用户自定义数据缓存
};

void app_main(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());
	init_camera();
	wifi_start(&wifi);
}

void on_wifi_callback(wifi_event_type_t event){
    if(event == EVENT_GOT_IP){
    	do_start_http_client();
    }

    if(event == EVENT_DISCONNECT){

    }
}

// void http_client_task(void* params){
// 	esp_http_client_handle_t client = (esp_http_client_handle_t)params;

// 	//进行链路的连接与数据的传输，内部包含完整的收发断连的API调用
// 	//需要在配置esp_http_client_init后调用运行
// 	esp_err_t err = esp_http_client_perform(client);

// 	if (err == ESP_OK) {
// 		ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
// 		                esp_http_client_get_status_code(client),
// 		                esp_http_client_get_content_length(client));
// 	} else {
// 		ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
// 	}
// 	esp_http_client_cleanup(client);
// 	vTaskDelete(NULL);
// }

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


//post
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
