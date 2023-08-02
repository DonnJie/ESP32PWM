#ifndef _LEDC_H_
#define _LEDC_H_

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "gatts_server.h"


#define LEDC_HS_TIMER LEDC_TIMER_0 //定时器0
#define LEDC_HS_MODE LEDC_LOW_SPEED_MODE //低速模式
#define LEDC_HS_CH0_GPIO GPIO_NUM_7  //输出引脚
#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0 //LED通道

#define LEDC_TIMER_FREQ_HZ 5000 //PWM的频率



void led_pwm(uint8_t);
void led_task(void *);

#endif