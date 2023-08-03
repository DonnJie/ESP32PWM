
#include "rgb_rmt.h"

rmt_channel_handle_t led_chan = NULL; // RMT通道句柄
rmt_encoder_handle_t led_encoder = NULL; // LED灯带编码器句柄

 /*
    h代表色彩， s代表颜色的深浅， v代表颜色的亮度
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 256; // 将色相值限制在 [0, 255] 的范围内                          //色相值范围，自己定义
    uint32_t rgb_max = v; // 最大RGB值                                    //uint32_t rgb_max = v * 255 / 100; // 最大RGB值
    uint32_t rgb_min = rgb_max * (255 - s) / 255; // 最小RGB值            // uint32_t rgb_min = rgb_max * (100 - s) / 100; // 最小RGB值
    uint32_t i = h / 43; // 计算所在的色相区间                              //色相值乘以6除以范围  256/6 = 43
    uint32_t diff = h % 43; // 计算色相在该区间内的偏移量

    // 根据色相调整RGB值
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 43;

    switch (i){
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void rmt_rgb_init()
{
        ESP_LOGI(TAG, "Create RMT TX channel"); // 输出日志
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // 选择时钟源
        .gpio_num = RMT_LED_STRIP_GPIO_NUM, // GPIO引脚号
        .mem_block_symbols = 64, // 增加块大小可以使LED灯闪烁更少
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ, // 分辨率
        .trans_queue_depth = 4, // 设置可以在后台挂起的事务数
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan)); // 创建RMT TX通道

    ESP_LOGI(TAG, "Install led strip encoder"); // 输出日志
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder)); // 安装LED灯带编码器
    ESP_LOGI(TAG, "Enable RMT TX channel"); // 输出日志
    ESP_ERROR_CHECK(rmt_enable(led_chan)); // 启用RMT TX通道
    ESP_LOGI(TAG, "Start LED rainbow chase"); // 输出日志
}

void rmt_rgb(uint8_t mode,uint32_t rgb_num1,uint32_t rgb_num2,uint32_t rgb_num3)
{
    uint32_t red = rgb_num1; // 红色RGB值
    uint32_t green = rgb_num2; // 绿色RGB值
    uint32_t blue = rgb_num3; // 蓝色RGB值
    uint32_t hue = rgb_num1; // 色相值
    uint32_t saturation = rgb_num2; //饱和度
    uint32_t value = rgb_num3; //亮度

        rmt_transmit_config_t tx_config = {
        .loop_count = 0, // 不进行传输循环
    };
    if(mode ==1)
        {
            for (int i = 0; i < 3; i++) 
            {
                for (int j = i; j < EXAMPLE_LED_NUMBERS; j += 3) 
                {
                    // 指定颜色
                    // hue = 10;      // 色调（0-360度）
                    // saturation = 100;  // 饱和度（0-100）
                    // value = 1;
                    
                    // 将HSV颜色转换为RGB颜色
                    led_strip_hsv2rgb(hue, saturation, value, &red, &green, &blue);
                    led_strip_pixels[j * 3 + 0] = green;
                    led_strip_pixels[j * 3 + 1] = red;
                    led_strip_pixels[j * 3 + 2] = blue;
                }
                // 将RGB值刷新到LED灯带上
                ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
                ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
                //vTaskDelay(pdMS_TO_TICKS(10)); // 延时一段时间
            }
        }
        else
        {
            for (int i = 0; i < 3; i++) 
            {
                for (int j = i; j < EXAMPLE_LED_NUMBERS; j += 3) 
                {
                    // 指定RGB值
                    // red = 255;    // 红色（0-255）蓝？
                    // green = 0;    // 绿色（0-255）
                    // blue = 0;     // 蓝色（0-255）?红

                    led_strip_pixels[j * 3 + 0] = green;
                    led_strip_pixels[j * 3 + 1] = red;
                    led_strip_pixels[j * 3 + 2] = blue;
                }
                // 将RGB值刷新到LED灯带上
                ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
                ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
                //vTaskDelay(pdMS_TO_TICKS(10)); // 延时一段时间
            }
        }
}

void rgb_rmt_task(void *arg)
{
    rmt_rgb_init();
    uint8_t* rgb_num;

    
    while (1) 
    {
        rgb_num = get_rgb_num();
        rmt_rgb(rgb_num[0],rgb_num[1],rgb_num[2],rgb_num[3]);
        //printf("test-1\n");
        vTaskDelay(pdMS_TO_TICKS(20));
        free(rgb_num);
    }

    // while (1)
    // {
    //     /* code */
    //     printf("test-1\n");
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    
}




