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
system_parameter_t system_parameter;
historyRecord_t historyRecord[num_historyRecord];
uint32_t   UTCCurrentTime = 0;
char     flag_server = 0;
char     is_lack_water = 0;

uint32_t  his_starttime;
uint32_t  his_worktime;
char      flag_his_Yes = 0;
char      is_work = 0;
char      flag_his_write_flash = 0;

uint32_t  MAX_POWER = 400;
//最大功率有下面几种，但是加热管一根是400W
//200/400/600/800/1000/1200
uint32_t getduty()
{
    switch(MAX_POWER)
    {
        case 200:      //一根管
            return 50;
        break;
        case 400:      //一根管
            return 100;
        break;
        case 600:      //两根管
            return 75;
        break;
        case 800:      //两根管
            return 100;
        break;
        case 1000:
            return 83;
        break;
        case 1200:
            return 100;
        break;
        default:       //异常
            return 100;
        break;
    }
}


//获取服务器状态 
char GetServerStatus()
{
    return flag_server;
}

//读取时间
uint32_t GetCurrentTime(void)
{
    UTCCurrentTime = return_current_time();   
    return UTCCurrentTime;
}



#define  HISTORY   0xBB
#define  ONOFF     0xAA
#define  SETTEM    0xAB
#define  DELETE    0xAF
#define  CURRENT   0xAD

//上报历史记录
void reporthistory(uint32_t starttime,uint32_t worktime)
{
    uint8_t  reply[20];
    reply[0] = HISTORY;
    reply[1] = 0x00;
    reply[2] = 0x08;
    reply[3] = starttime >> 24;
    reply[4] = starttime >> 16;
    reply[5] = starttime >> 8;
    reply[6] = starttime;
    reply[7] = worktime >> 24;
    reply[8] = worktime >> 16;
    reply[9] = worktime >> 8;
    reply[10]= worktime;
    senddatatoserver(reply,11);
}
//上报版本号
extern char *  returnversion();
void reportsoftversion()
{
    char * str = returnversion();
    uint8_t array[14];
    array[0] = 0xC5;
    array[1] = 0x03;
    array[2] = 0x0A;
    array[3] = str[0];
    array[4] = str[1];
    array[5] = str[2];
    array[6] = str[3];
    array[7] = str[4];
    array[8] = str[5]; 
    array[9] = str[6];
    array[10] = str[7];
    array[11] = str[8];
    array[12] = str[9];  
    array[13] = str[10]; 
    senddatatoserver(array,14);
}
//上报所有状态
void reportall()
{
    uint8_t  reply[20];
    reply[0] = CURRENT;
    reply[1] = 0x00;
    reply[2] = 0x09; 

    reply[3] = system_parameter.onoff;
    reply[4] = is_work;
    reply[5] = system_parameter.set_temperature;
    reply[6] = temperature_avr / 256;   
    reply[7] = temperature_avr % 256;   
    reply[8] = is_lack_water;
    reply[9] =  return_temp_power() >> 8;
    reply[10] = return_temp_power();
    reply[11] = MAX_POWER >> 8;
    reply[12] = MAX_POWER;
    senddatatoserver(reply,13);
}
void report_current()
{
    static uint32_t tick;
 
    tick ++;

    if(tick >= 30 * 10 * 60)     //30分钟
    {
        if(GetServerStatus())
        {
            reportall();
            tick = 0; 
        }
    }
}

/************************************************************
 * 函数名称：void StateUpdate(void)
 * 函数说明：当前状态上报
 * 变量输入：
 * 函数输出：无
*************************************************************/
void outlogi(uint8_t * data,uint16_t len)
{
    // if (1) {
    //     char buf[512];
    //     char *p = buf;
    //     for (int i = 0; i < len; i++) {
    //         sprintf(p, "%02x ", data[i]);
    //         p = p + 3;
    //     }
    //     *p = 0;
    //     LOG("%s",buf);
    // }    
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

void senddatatoserver(uint8_t * temp,uint8_t len)
{
    LOG("send data");
    outlogi(temp,len);
    MsgsSendToServer(temp,len);
}

//收到数据处理
//extern int MsgsSendToServer(const unsigned char *pdata, unsigned char len);
void bump_receive_server(unsigned char *data, int len)
{
    switch (data[0])
    {
        case ONOFF:
            if(data[1] == 1)
            {
                system_parameter.onoff = data[3];
                Writesysappointment();
//                ESP_LOGI(TAG, "system_parameter.onoff = %d",system_parameter.onoff);
            }
        break;

        case SETTEM:
            if(data[1] == 1)
            {
                system_parameter.set_temperature = data[3];
                Writesysappointment();
            }  
        break;

        // case DELETE:
        //     ESP_LOGI(TAG, "key1 press, netconfig init");
        //     netconfig_init();
        //     usleep(1000*1000);              
        // break; 
        case CURRENT:
//            extern void reportall();
            reportall();
        break;
        default:
            break;
    }
}

//LED控制函数
void LEDProcess()
{
    static int gtick;
    if(system_parameter.onoff)
    {
        switch(get_net_state())
        {
            case RECONFIGED:
                ++ gtick;
                if (gtick > 30) {  // 0.3S
                    gtick = 0;
                    LED_Control(1);
                } else if (gtick > 5) {
                    LED_Control(0);
                }
                flag_server = 0;
            break;
            case CONNECT_CLOUD_SUCCESS:
                LED_Control(1);
                flag_server = 1;
            break;
            case UNCONFIGED:
                ++ gtick;
                if (gtick > 60) { 
                    gtick = 0;
                    LED_Control(1);
                } else if (gtick > 10) {
                    LED_Control(0);
                }
                flag_server = 0;
            break;
            default:
                ++ gtick;
                if (gtick > 400) {  // 0.3S
                    gtick = 0;
                    LED_Control(1);
                } else if (gtick > 200) {
                    LED_Control(0);
                }
                flag_server = 0;
            break;
        }
    }
    else
    {
        LED_Control(0);
    }
}
//检查状态
void check_is_work()
{
    static uint8_t temp = 0;
    if(system_parameter.onoff != temp)
    {
        // extern void restart_senser();
        // restart_senser();   //每次继电器状态出现改变，就重启一次传感器
        temp = system_parameter.onoff;
        if(temp == 1)     //由关变成开
        {
            if (GetTimeState()) 
            {
                his_starttime = return_current_time();
            }
            else
            {
                his_starttime = 0;
            }
        }
        else
        {
            if(his_starttime > 0)   //第一个字节不对不处理
            {
                his_worktime = return_current_time() - his_starttime;
                flag_his_Yes = 1;        //产生历史记录
            }
        }
    }
}
//上报所有历史记录
void report_his_all()
{
    for(int i = 0;i < num_historyRecord; i++)
    {
        if(historyRecord[i].starttime > Standard_timestamp)
        {
            reporthistory(historyRecord[i].starttime,historyRecord[i].worktime);
            historyRecord[i].starttime = 0;
            historyRecord[i].worktime = 0;
            aos_msleep(100);   //延迟10ms
        }
    }
}
//上报历史记录
void is_report_his()
{
    if(flag_his_Yes)
    {
        if(GetServerStatus())
        {
            reporthistory(his_starttime,his_worktime);
        }
        else
        {
            SaveHistoryRecord(his_starttime,his_worktime);
            flag_his_write_flash = 1;
        }
        flag_his_Yes = 0;
    }

    if(flag_his_write_flash)
    {
        if(GetServerStatus())
        {
            report_his_all();    //上报所有记录
            flag_his_write_flash = 0;
        }
    }
}
//按键处理
void Key1_Press_Cb(char i)
{
    if(i)
    {
        LOG("key1 press, netconfig init");
        save_ssid_password(0,"","");
//        aos_msleep(1000);  
        os_reboot();  
    }
    else
    {
        if(system_parameter.onoff)
        {
            system_parameter.onoff = 0;
        }
        else
        {
            system_parameter.onoff = 1;
        }
        Writesysappointment();     
    }
}

//存储最大只10条，超过10条必须找出最早那条,然后覆盖他
static uint8_t FindMin(historyRecord_t * temp)
{
    uint8_t i,j = 0;
    uint32_t min = temp[0].starttime;
    for(i = 1;i < num_historyRecord; i++)
    {
        if(temp[i].starttime < min)
        {
            min = temp[i].starttime;
            j = i;   //将最小值的下标保存
        }
    }
    return j;
}
//存储记录
void SaveHistoryRecord(uint32_t x,uint32_t y)
{
    uint8_t i;
    i = FindMin(&historyRecord[0]);  //算出最小值，覆盖
    historyRecord[i].starttime = x;
    historyRecord[i].worktime = y;
    Writerecord();
}

static uint32_t restart_tick = 0;
// void restart_senser()  //直接重启
// {
//     restart_tick = 500;
// }
// void check_no_water()
// {
//     restart_tick ++;
//     if(restart_tick <= 500)
//     {
//         if((restart_tick % 100) == 0)   //秒的整数
//         {
//             if(read_iswater_value() == 1)    //高电平有水
//             {
//                 is_lack_water = 0;
//                 LOG("is_lack_water = 0"); 
//             }
//             else
//             {
//                 is_lack_water = 1;             //缺水
//                 ONOFF_Control(0);                 //缺水的时候需要立即停止
//                 LOG("is_lack_water = 1"); 
//             }            
//         }
//     }
//     else if(restart_tick <= 600)   //停1秒
//     {
//         POWER_ON_OFF(0);     
//     }
//     else
//     {
//         POWER_ON_OFF(1); 
//         restart_tick = 0;
//     }
// }

// uint32_t get_is_water()
// {
//     return read_iswater_value();
// }
