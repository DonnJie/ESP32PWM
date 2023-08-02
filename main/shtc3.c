#include "shtc3.h"

void shtc3_i2c_master_init()
{
    //配置I2C总线，配置主机
    i2c_config_t conf; //定义一个I2C的结构体
    conf.mode = I2C_MODE_MASTER;  //工作模式为主机模式
    conf.sda_io_num = I2C_MASTER_SDA_IO; // 定义SDA引脚
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE; //启用内部上拉电阻
    conf.scl_io_num = I2C_MASTER_SCL_IO; //定义SCL引脚
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE; //启用内部上拉电阻
    conf.master.clk_speed = 100000; // 100khz ，标准速度模式，400 kHz，快速速度模式 ，1 MHz高速速度模式 3.4 MHz 超高速速度模式
    i2c_param_config(I2C_MASTER_NUM, &conf);//配置I2C总线参数,主要I2C总线号
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);// 安装并启动I2C驱动程序
}

static float temperature, humidity; //温度和湿度数据实际值 


void shtc3_task(void *arg)
{
    //在主机模式下通信
    uint16_t temp, humi;  // 温度和湿度数据原始值

    shtc3_i2c_master_init(); // 初始化I2C总线
    while(1) {
        //https://docs.espressif.com/projects/esp-idf/zh_CN/v5.0.3/esp32c3/api-reference/peripherals/i2c.html?highlight=i2c
        // 主机写入数据
        uint8_t cmd[2] = {SHTC3_CMD_MEASURE >> 8, SHTC3_CMD_MEASURE & 0xFF};
        i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
        i2c_master_start(cmd_handle);//发送起始位
        i2c_master_write_byte(cmd_handle, SHTC3_ADDR << 1 | I2C_MASTER_WRITE, true);//发送设备地址和写命令
        i2c_master_write(cmd_handle, cmd, sizeof(cmd), true);// 发送测量命令
        i2c_master_stop(cmd_handle); // 发送停止位
        i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, pdMS_TO_TICKS(100));// 等待传感器响应
        i2c_cmd_link_delete(cmd_handle); // 删除命令对象
        vTaskDelay(pdMS_TO_TICKS(100));// 等待传感器完成测量，1秒

        // 主机读取数据
        uint8_t data[6];
        cmd_handle = i2c_cmd_link_create();
        i2c_master_start(cmd_handle);// 发送起始位
        i2c_master_write_byte(cmd_handle, SHTC3_ADDR << 1 | I2C_MASTER_READ, true); // 发送设备地址和读命令
        i2c_master_read(cmd_handle, data, sizeof(data), I2C_MASTER_LAST_NACK); // 读取传感器数据
        i2c_master_stop(cmd_handle);// 发送停止位
        i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, pdMS_TO_TICKS(100));  // 等待传感器响应
        i2c_cmd_link_delete(cmd_handle);// 删除命令对象
         // 解析传感器数据
        temp = (data[0] << 8) | data[1];//将温度高八位和第八位拼接起来，接下来的八位是保留位
        humi = (data[3] << 8) | data[4];
        // 根据温度原始值计算实际温度值
        temperature = -45.0f + 175.0f * (float)temp / 65535.0f; //计算公式来源于数据手册
        // 根据湿度原始值计算实际湿度值
        humidity = 100.0f * (float)humi / 65535.0f;
         // 打印传感器数据
        // printf("Temperature: %.2f C\t", temperature);
        // printf("Humidity: %.2f %%RH\n", humidity);
        // 每5秒读取一次传感器数据
        // vTaskDelay(5000 / portTICK_PERIOD_MS); // 每5秒读取一次传感器数据
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//获取温度值
float getTemperature() 
{
    return temperature;
}

//获取湿度值
float getHumidity()
{
    return humidity;
}
