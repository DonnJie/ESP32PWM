
#include "shtc3.h"
#include "ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "gatts_server.h"
#include "rgb_rmt.h"
#include "oneshot_adc.h"



void app_main(void)
{
    Bluetooth_init();
    // 初始化 LEDC 驱动程序库
    ledc_fade_func_install(0);
    // 创建读取传感器数据的任务
    xTaskCreate(shtc3_task, "shtc3_task", 2048, NULL, 5, NULL);
    //创建ADC采集任务
    xTaskCreate(oneshot_adc_task,"oneshot_adc_task",2048,NULL,5,NULL);
    // 创建 LED 任务
    xTaskCreate(led_task, "LED Task", 2048, NULL, 4, NULL); 
    xTaskCreate(rgb_rmt_task, "RGB_RMT_task", 2048, NULL, 4, NULL);

}
