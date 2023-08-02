#ifndef _RGB_RMT_H_
#define _RGB_RMT_H_


#include "rgb_strip_encoder.h"
#include <string.h> //导入字符串处理函数库
#include "freertos/FreeRTOS.h" //导入FreeRTOS操作系统库
#include "freertos/task.h" //导入FreeRTOS任务库
#include "esp_log.h" //导入ESP日志库
#include "driver/rmt_tx.h" //导入RMT发射器库
#include "rgb_strip_encoder.h" //导入LED灯带编码器库
#include "gatts_server.h"


#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // RMT发射器的分辨率，10MHz分辨率，1个时钟周期=0.1微秒（LED灯带需要高分辨率）
#define RMT_LED_STRIP_GPIO_NUM 2 // RMT发射器连接的GPIO引脚号
#define EXAMPLE_LED_NUMBERS 1 // LED灯的数量
#define EXAMPLE_CHASE_SPEED_MS 1 // 彩虹跑的速度，单位为毫秒

static const char *TAG = "example"; // 日志标签
static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3]; // 存储LED灯的RGB值



void rgb_rmt_task(void *arg);


#endif