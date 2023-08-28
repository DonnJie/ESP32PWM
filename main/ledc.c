#include "ledc.h"

void led_pwm(uint8_t brightness)
{
    // 配置 GPIO7 为 PWM 输出引脚
    esp_rom_gpio_pad_select_gpio(GPIO_NUM_7);
    gpio_set_direction(GPIO_NUM_7, GPIO_MODE_OUTPUT);

    // 配置 LEDC 定时器
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT, // 设置PWM分辨率，8位即可以有256个不同占空比 
        .freq_hz = LEDC_TIMER_FREQ_HZ,       // PWM信号的频率，PWM波形的周期，频率越高LED灯看起来会越稳定，使得LED灯亮度变化更快
        .speed_mode = LEDC_HS_MODE,          // LEDC模块的速度模式，低速模式适用于高精度的PWM信号场合，高速模式适用于较高PWM信号频率的场合
        .timer_num = LEDC_HS_TIMER};         //  LEDC定时器的编号。ESP32C3的LEDC模块提供了4个定时器，LEDC_TIMER_0~LEDC_TIMER_3，分别可以独立地产生PWM，定时器可以控制多个PWM通道
    ledc_timer_config(&ledc_timer);          //

    // 配置 LEDC 通道,多个通道就定义多个数组
    ledc_channel_config_t ledc_channel[1]= {
        {.channel = LEDC_HS_CH0_CHANNEL,           // LED通道的编号
         .duty = (uint32_t)brightness, // 表示PWM信号的占空比，
         .gpio_num = LEDC_HS_CH0_GPIO,             // 表示对应LED通道的GPIO引脚编号
         .speed_mode = LEDC_HS_MODE,               // 表示LEDC模块的速度模式
         .timer_sel = LEDC_HS_TIMER},         // 表示LED通道使用的定时器编号
    }; 
    ledc_channel_config(&ledc_channel[0]); // 将ledc_channel数组中的参数应用到LEDC模块中，完成了LED通道的配置。
    //ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL,get_brightness()); //设置默认占空比，即默认亮度
    // 更新 PWM 信号的占空比
    ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL); // 将LED通道中的占空比参数更新到PWM信号中，从而控制LED灯的亮度
}

void led_task(void *pvParameters)
{
   // uint8_t brightness = 255; // 37, 175, 243
   // u_int8_t temp = 1;
    while (1)
    {   
        led_pwm(get_brightness());
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
