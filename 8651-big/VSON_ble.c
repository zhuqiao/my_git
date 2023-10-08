#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "aos/kernel.h"
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "breeze_export.h"
#include "combo_net.h"

//打印输出
static void OutLogi(unsigned char *data, int len)
{
    char buf[512];
    LOG("len = %d:", len);  
    // char *p = buf;
    // for (int i = 0; i < len; i++) {
    //     sprintf(p, "%02x ", data[i]);
    //     p = p + 3;
    // }
    // *p = 0;
    // LOG("rec data:", "%s", buf);   
    printf("rec data :");
    for(int i = 0 ;i < len ; i++)
    {
        printf("%02x " , data[i]);
    } 
    printf("\n");
}
//获取mac地址，并转换格式
uint8_t reply[10];
extern uint8_t * read_mac_address();
uint8_t * sendmacaddress()
{ 
    reply[0] = 'm';
    reply[1] = 'a';
    reply[2] = 'c';
    reply[3] = ':';

    memcpy(&reply[4],read_mac_address(),6);
    return reply;
}
//蓝牙发送wifi帐号密码
static char ssid[32];
static char passwd[64];
static char ble_rec_sucess = 0; //wifi帐号密码接收成功
extern void ble_send_data(uint8_t *p_data, uint16_t length);
extern void save_ssid_password(char flag , char * ssid , char * password);

void ble_recdata_process(char * data, uint8_t len)
{
    static uint8_t step; 
    static uint8_t flag;
    OutLogi((uint8_t*)data,len);
    ble_send_data((uint8_t*)data,len); 
//    LOG("rec string: %s\n",data);
    if(strstr(data, "vson_ssid") != NULL)
    {
        step = 1;      //如果收到vson_ssid，说明刚开始收到指令
        LOG("step = 1\n");
        memset(ssid,0,sizeof(ssid));
        flag = 0;
        return;     //防止执行下面函数
    }
    else if(strstr(data, "ssid:end") != NULL)   //SSID接收完成
    {
        flag = 1;
        LOG("ssid: %s\n",ssid);
        step = 0;
        return;
    }
    else if(strstr(data, "vson_password") != NULL)
    {
        step = 2;      //如果收到vson_password，说明开始接收密码
        LOG("step = 2\n");
        memset(passwd,0,sizeof(passwd));
        return;     //防止执行下面函数
    }
    else if(strstr(data, "password:end") != NULL)   //密码接收完成
    {
        if(flag)
        {
            ble_send_data(sendmacaddress(),10);  //发送mac地址
            LOG("delay 5 s\n");
//            usleep(1000 * 1000 * 5);
            LOG("password: %s\n",passwd);
            step = 0;
            flag = 0;
            ble_rec_sucess = 1;
            LOG("ssid and password Receive All\n");
//            xTaskCreate(deleteble, "deleteble", 4096, NULL, 3, NULL);
//            bt_disable();       //关闭蓝牙   
            save_ssid_password(1 , ssid , passwd);    //保存帐号密码   
            extern void vson_stop_awss();
            vson_stop_awss();
        }
        else
        {
            LOG("flag = 0\n");
        }
        
        return;
    }
    switch(step)
    {
        case 1:
            if((data[0] == 1) || (data[0] == 2))
            {
                memcpy(&ssid[(data[0] - 1) * 19],&data[1],len - 1);   //存储账号密码
                LOG("ssid1: %s\n",ssid);
            }
            else
            {
                LOG("ssid data[0] id error\n");
            }
        break;

        case 2:
            if((data[0] > 0x10) && (data[0] < 0x15))
            {
                memcpy(&passwd[(data[0] - 0x11) * 19],&data[1],len - 1);   //存储账号密码
                LOG("passwd1: %s\n",passwd);
            }
            else
            {
                LOG("password data[0] id error\n");
            }
        break;
        default:break;
    }
}



