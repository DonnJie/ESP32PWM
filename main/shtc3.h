#ifndef _SHTC3_H_
#define _SHTC3_H_

#include "sdkconfig.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"



#define SHTC3_ADDR          0x70    // SHTC3 I2C 地址
#define SHTC3_CMD_MEASURE   0x7CA2  // SHTC3 测量命令

#define I2C_MASTER_SCL_IO   8       // GPIO8作为SCL线
#define I2C_MASTER_SDA_IO   10      // GPIO10作为SDA线
#define I2C_MASTER_NUM      I2C_NUM_0  // I2C总线号


void shtc3_i2c_master_init();
void shtc3_task(void*);

float getTemperature();
float getHumidity();


#endif