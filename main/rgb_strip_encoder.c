#include "esp_check.h"
#include "rgb_strip_encoder.h"

static const char *TAG = "led_encoder";

typedef struct {
    rmt_encoder_t base;                   // RMT编码器基础结构体
    rmt_encoder_t *bytes_encoder;         // 用于编码RGB数据的字节编码器
    rmt_encoder_t *copy_encoder;          // 用于编码复制数据（重置代码）的编码器
    int state;                            // 当前编码状态
    rmt_symbol_word_t reset_code;         // 重置代码
} rmt_led_strip_encoder_t;                // 自定义的led灯带编码器结构体

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);  // 获取led灯带编码器结构体
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;                               // 获取字节编码器
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;                                 // 获取复制编码器
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;                                         // 当前会话的编码状态
    rmt_encode_state_t state = RMT_ENCODING_RESET;                                                 // 当前编码状态
    size_t encoded_symbols = 0;
    switch (led_encoder->state) {
    case 0: // 发送RGB数据
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);   // 编码RGB数据
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; // 当前编码会话完成后进入下一个状态
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;  // 如果没有编码器空间，则设置编码状态
            goto out; // 如果没有编码器空间，则退出
        }
    // fall-through
    case 1: // 发送重置代码
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);  // 编码重置代码
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = RMT_ENCODING_RESET; // 返回初始编码状态
            state |= RMT_ENCODING_COMPLETE;          // 如果编码完成，则设置编码状态
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;  // 如果没有编码器空间，则设置编码状态
            goto out; // 如果没有编码器空间，则退出
        }
    }
out:
    *ret_state = state;  // 将编码状态返回
    return encoded_symbols;  // 返回已编码的符号数
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);  // 获取led灯带编码器结构体
    rmt_del_encoder(led_encoder->bytes_encoder);   // 删除字节编码器
    rmt_del_encoder(led_encoder->copy_encoder);    // 删除复制编码器
    free(led_encoder);                             // 释放编码器内存
    return ESP_OK;                                 // 返回成功
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);  // 获取led灯带编码器结构体
    rmt_encoder_reset(led_encoder->bytes_encoder);   // 重置字节编码器
    rmt_encoder_reset(led_encoder->copy_encoder);    // 重置复制编码器
    led_encoder->state = RMT_ENCODING_RESET;         // 将编码器状态设置为初始状态
    return ESP_OK;                                  // 返回成功
}

esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;   // 声明led灯带编码器结构体指针并初始化为NULL
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");   // 检查参数是否正确
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));   // 分配led灯带编码器结构体内存
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");   // 检查是否成功分配内存
    led_encoder->base.encode = rmt_encode_led_strip;   // 将编码函数指针指向rmt_encode_led_strip函数
    led_encoder->base.del = rmt_del_led_strip_encoder;  // 将删除函数指针指向rmt_del_led_strip_encoder函数
    led_encoder->base.reset = rmt_led_strip_encoder_reset;  // 将重置函数指针指向rmt_led_strip_encoder_reset函数
    // 不同的LED灯带可能具有其自己的时序要求，以下参数适用于WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
            .level1 = 0,
            .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
            .level1 = 0,
            .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
        },
        .flags.msb_first = 1 // WS2812传输的位顺序为：G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");   // 创建字节编码器并检查是否成功
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");   // 创建复制编码器并检查是否成功

    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2;   // 重置代码持续时间默认为50us
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };
    *ret_encoder = &led_encoder->base;   // 将编码器指针返回给调用者
    return ESP_OK;
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);   // 删除字节编码器
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);   // 删除复制编码器
        }
        free(led_encoder);   // 释放编码器内存
    }
    return ret;   // 返回错误代码
}
