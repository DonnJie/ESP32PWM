#ifndef _ONESHOT_ADC_H_
#define _ONESHOT_ADC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


/**
 * 定义一个日志标签，用于标记所有的 log 信息。
 */
const static char *TAG_ADC = "oneshot_adc";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
//ADC1 Channels
/**
 * 定义 ADC1 的通道。对于 ESP32，我们使用 ADC_CHANNEL_4 和 ADC_CHANNEL_5；
 * 对于 ESP32C3，我们使用 ADC_CHANNEL_2 和 ADC_CHANNEL_3。
 */
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_2
// #define EXAMPLE_ADC1_CHAN1          ADC_CHANNEL_3

/**
 * 对于 ESP32C3，由于其硬件限制，不再支持 ADC2。如果设备支持 ADC2，
 * 我们会定义 EXAMPLE_USE_ADC2 标志，并定义相应的通道。
 */

#define EXAMPLE_USE_ADC2            0

#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_11

/**
 * 定义一个衰减系数（ADC_ATTEN_DB_11），并初始化了两个用于存储 ADC 原始数据和电压值的数组。
 * 二维数组，实际是因为定义了两个ADC通道
 */
static int adc_raw[2][10];
static int voltage[2][10];

static bool example_adc_calibration_init(adc_unit_t, adc_atten_t, adc_cali_handle_t *);
static void example_adc_calibration_deinit(adc_cali_handle_t);
void oneshot_adc_task(void *);





#endif
