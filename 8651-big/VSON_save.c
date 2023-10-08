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
#include <hal/soc/gpio.h>
#include "vendor.h"
#include "device_state_manger.h"
#include "iot_import_awss.h"
#include "smart_outlet.h"
#include "VSON_common.h"

#define eepromFlag       0xAA

#define KEYSYSTEM        "key_system"
#define KEYRECORD        "key_record"
#define KEYMAXPOWER      "key_maxpower"

//读参数
extern  uint32_t  MAX_POWER;
void readmaxpower()
{
    int ret;
    uint16_t len = sizeof(uint32_t);
    ret = aos_kv_get(KEYMAXPOWER, &MAX_POWER, &len);
    if(ret != 0)    //读取失败
    {
        MAX_POWER = 400;
        LOG("read eeprom eeror3");
        return;
    }
}
//写参数
int Writemaxpower()
{
    if(aos_kv_set(KEYMAXPOWER, &MAX_POWER, sizeof(uint32_t),0) == 0)
    {
        LOG("write eeprom success");
        return 0;
    }
    LOG("write eeprom eeror");
    return -1;
}

//读参数
void readsysappointment()
{
    int ret;
    uint16_t len = sizeof(system_parameter_t);
    ret = aos_kv_get(KEYSYSTEM, &system_parameter, &len);
    if(ret != 0)    //读取失败
    {
        system_parameter.flag = eepromFlag;
        system_parameter.set_temperature = 25;   
        system_parameter.onoff = 0;
        LOG("read eeprom eeror1");
        return;
    }
    if(system_parameter.flag != eepromFlag)
    {
        system_parameter.flag = eepromFlag;
        system_parameter.set_temperature = 25;   
        system_parameter.onoff = 0;
        LOG("read eeprom eeror1");
        return;
    }
}
//写参数
int Writesysappointment()
{
    if(aos_kv_set(KEYSYSTEM, &system_parameter, sizeof(system_parameter_t),0) == 0)
    {
        LOG("write eeprom success");
        return 0;
    }
    LOG("write eeprom eeror");
    return -1;
}
void clearsysappointment()
{
    system_parameter.flag = eepromFlag;
    system_parameter.set_temperature = 25;   
    system_parameter.onoff = 0;
    Writesysappointment();
}
// //水泵运行记录
void readrecord()
{
    int len = sizeof(historyRecord);
    if( aos_kv_get(KEYRECORD, historyRecord, &len) != 0 )   //读取数据失败
    {
        memset(&historyRecord, 0, sizeof(historyRecord));   //读取失败，将所有数据清零
        LOG("read eeprom eeror1");
    } 
}
int Writerecord()
{
    if(aos_kv_set(KEYRECORD, historyRecord, sizeof(historyRecord), 0) == 0)  //存储
    {
        return -1;
    }
    LOG("write eeprom eeror2");
    return 0;
}
void clearrecord()
{
    memset(historyRecord, 0, sizeof(historyRecord));   //读取失败，将所有数据清零
    Writerecord();
}

//初始化函数
void UserConfigInit(void)
{
    readsysappointment();
    readrecord();
    readmaxpower();
}//将所有存储的数据全部清零
void clearFlash(void)
{
    clearsysappointment();
    clearrecord();
}