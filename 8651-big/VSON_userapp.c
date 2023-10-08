#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <aos/aos.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "app_entry.h"
#include "aos/kv.h"
#include "vendor.h"
#include "device_state_manger.h"
#include "smart_outlet.h"
#include "msg_process_center.h"
#include "property_report.h"
#include "VSON_common.h"
//#include <time.h>

uint32_t CRUENT_TIME = 0;    //当前时间戳
my_time_t my_time = {0};

//发送数据
void MsgsSendToServer(uint8_t * temp,uint8_t len)
{
    int res;    //返回0发送成功
    res = IOT_Linkkit_Report(0, ITM_MSG_POST_RAW_DATA,
                             temp, len);
}

// my_time_t * time_conversion() 
// {
//     my_time.sec = CRUENT_TIME%60;      
//     my_time.min = CRUENT_TIME / 60 % 60; 
//     my_time.hour =  CRUENT_TIME / 3600 % 24;  
//     my_time.hour += 8;
//     if(my_time.hour > 23)
//     {
//         my_time.hour -= 24;
//     }
//     my_time.week = (CRUENT_TIME / 3600 / 24 + 4) % 7;  //   加4是因为当天星期7
//     if(my_time.week == 0)  my_time.week = 7;
//     return &my_time;
// }
my_time_t * time_conversion() 
{
    my_time.sec = CRUENT_TIME%60;      
    my_time.min = CRUENT_TIME / 60 % 60; 
    my_time.hour =  CRUENT_TIME / 3600 % 24;  
    my_time.week = (CRUENT_TIME / 3600 / 24 + 4) % 7;  //   加4是因为当天星期4

    my_time.hour += 8;
    if(my_time.hour > 23)
    {
        my_time.hour -= 24;

        my_time.week ++;
        if(my_time.week > 6)
            my_time.week = 0;
    }

    if(my_time.week == 0)  my_time.week = 7;
    return &my_time;
}

void gettime()
{
    //时间戳每秒+1
    static uint64_t temp;
    uint64_t now_ms = aos_now_ms() / 1000;
    if(temp != now_ms)    //秒出现变化
    {
        temp = now_ms;
        CRUENT_TIME ++;
    }
}

uint32_t return_current_time()
{
    return CRUENT_TIME;
}

static char first = 1;
static uint32_t firsttick = 0;
void set_first_report(char i)
{
    first = 1;
    if(i == 0)
    {
        firsttick = 0;              //延迟上报
    } 
    else
    {
        firsttick = 30 * 10;         //直接上报
    }
}

//读取标志进程
void Msg_Report(void *param)
{   
    while (1)
    {
        if(GetServerStatus())
        {
            firsttick ++;
            if(first && (firsttick >= 30 * 10))
            {
                first = 0;
                reportall();
                reportsoftversion();
            }
        }
        else
        {
            
        }
        is_report_his();
        report_current();  

        aos_msleep(100);    
    }
}
//串口发送任务
extern  uint32_t  MAX_POWER;
void uart_send(void *param)
{
    uint8_t reply[6];
    while(1)
    {
    //uart send
        reply[0] = 0xAA;
        reply[1] = system_parameter.onoff;
        reply[2] = system_parameter.set_temperature;
        reply[3] = MAX_POWER / 256;
        reply[4] = MAX_POWER % 256;
        reply[5] = reply[0] + reply[1] + reply[2] + reply[3] + reply[4];
        uart_send_data(reply,6);   
        LOG("uart send data :%d %d %d %d %d %d",reply[0],reply[1],reply[2],reply[3],reply[4],reply[5]);
        aos_msleep(1000);  
    }
}

// static void testtest()
// {
//     static uint32_t tick = 0;
//     tick ++;
// //    LOG("tick = %d",tick);
//     if(tick > 99)
//     {
//         tick = 0;
//         LOG("uart send data3 :");
//     }
// }

static aos_task_t task_Msg_Report_process;
static aos_task_t task_uart_send;
extern char * returnversion();
void userapp(void *argv)
{
    Hardinit();    //硬件初始化
    UserConfigInit();
    LOG("verson = %s" , returnversion());
    aos_task_new_ext(&task_Msg_Report_process, "task_Msg_Report_process", Msg_Report, NULL, 4096, AOS_DEFAULT_APP_PRI);
    aos_task_new_ext(&task_uart_send, "task_uart_send", uart_send, NULL, 512, AOS_DEFAULT_APP_PRI);
    while(1)
    {
        Key_Scan();
        LEDProcess();
        check_is_work();
        gettime();
        user_uart_rec();
        feeddog();
//        aos_msleep(10);   
//       testtest();
    }
    aos_task_exit(0);
}


