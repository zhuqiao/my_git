#include <stdio.h>
#include <stdlib.h>
#include "VSON_common.h"
#include <hal/soc/gpio.h>
#include <hal/soc/adc.h>
#include <aos/log.h>
#include <hal/soc/uart.h>

// char Complete_measurement = 1;   //完成测量
void data_process(uint8_t * data ,uint8_t len);
void outputprocess();
extern uart_dev_t my_uart;
uint32_t  temperature_avr = 0;
uint32_t  temperature1 = 0;
uint32_t  temperature2 = 0;
uint32_t  temperature3 = 0;
// //#define   TEM_DIF     15
// //static uint8_t step = 1;        //第一次进入
// static uint8_t last_power = 0;  //最后一次功率
//串口发送数据
void uart_send_data(uint8_t * data , uint8_t len)
{
    hal_uart_send(&my_uart,data,len,10);
}
// //输出函数
//static uint8_t auot_power = 0;  //当前功率
uint8_t Flag_JDQ = 0;
//返回当前功率
// uint8_t read_current_power()
// {
//     return auot_power;
// }
// //PID调温
// #define ADD_VALUE1 10    //每次增加的值

// //计算功率
uint32_t power_temp = 0;
//uint8_t  power_average = 0;

// //返回临时功率
uint32_t return_temp_power()
{
    return power_temp;
}

// static void OutLogi1(unsigned char *data, int len)
// {
//     LOG("uart rec data :");
//     for(int i = 0 ;i < len ; i++)
//     {
//         LOG("%d" , data[i]);
//     } 
// }


uint8_t rec_buf[30];
static uint32_t recv_size;
uint32_t uart_tick = 0;
uint8_t  rec_num = 0;
uint8_t  system_buf[30];
#define  MAX_DATA_LEN 11
extern uint32_t  MAX_POWER;
void user_uart_rec()
{
    if(hal_uart_recv_II(&my_uart,&rec_buf,1,&recv_size,10) == 0)
    {
        uart_tick = 0;
        system_buf[rec_num ++] = rec_buf[0];
        if(rec_num >= sizeof(system_buf))
        {
            rec_num = 0;
        }
//        LOG("recv_size = %d,uart data = %d",recv_size,rec_buf[0]);
    }
    uart_tick ++;
    if(uart_tick > 10)  
    {
        uart_tick = 0;
        rec_num = 0;
    }
    if(system_buf[0] != 0xBB)
    {
        rec_num = 0;  
        return;      
    }
    if(rec_num > MAX_DATA_LEN)
    {
        rec_num = 0;
        LOG("data is over");
        return;
    }
    if(rec_num == MAX_DATA_LEN)
    {
        rec_num = 0;
        LOG("rec all data: %d %d %d %d %d %d %d %d %d %d %d",system_buf[0],system_buf[1],system_buf[2],system_buf[3],system_buf[4],system_buf[5],
                            system_buf[6],system_buf[7],system_buf[8],system_buf[9],system_buf[10]);
        //outlogi1(system_buf,MAX_DATA_LEN); 
        data_process(system_buf,MAX_DATA_LEN);
    }
}

void data_process(uint8_t * data ,uint8_t len)
{
    uint8_t sum = 0;
    for(int i = 0 ; i < len - 1; i ++)
    {
        sum += data[i];
    }
    if(sum != data[len - 1])
    {
        LOG("check sum error");
        return;
    }
    temperature1 = (uint32_t)system_buf[1] * 256 + system_buf[2];
    temperature2 = (uint32_t)system_buf[3] * 256 + system_buf[4];
    temperature3 = (uint32_t)system_buf[5] * 256 + system_buf[6];
    is_lack_water = system_buf[7];
    extern uint32_t power_temp;
    power_temp    = (uint32_t)system_buf[8] * 256 + system_buf[9];
    temperature_avr = (temperature1 + temperature2) / 2;
}



