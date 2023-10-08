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
#include <time.h>
#include "aos/network.h"
#include "VSON_common.h"

#define	INADDR_ANY		((in_addr_t) 0x00000000)
static aos_task_t task_app_tcp_process;
static aos_task_t task_app_client_process;
int serverfd, clientfd, len;
struct sockaddr_in servaddr, clientaddr;
int g_rxtx_need_restart = 0;   //是否需要重启
static char flag_creat = 0;
static char mServerip[30];

static char ssid[32];
static char passwd[64];

char VSON_Factory_flag = 0;

void app_client_process(void *argv);
extern uint32_t  MAX_POWER;
extern uint32_t  temperature1;
extern uint32_t  temperature2;

//数据处理函数
void rec_process(uint8_t * buf , uint8_t len)
{
    int idValid = 0;
    int match_param;
    int success = 0;
    uint8_t mac[6];
    memcpy(mac , read_mac_address() , 6);   //复制地址
    if ((mac[0] == 0)&&(mac[1] == 0)&&(mac[2] == 0)&&(mac[3] == 0)&&(mac[4] == 0)&&(mac[5] == 0)) 
    {
//        LOG("MAC has exist");
        idValid = 1;
    }
    if(idValid)   //没有mac地址
    {
        unsigned int tmpID[6];
        match_param = sscanf(buf, "[IDConfig] %02x:%02x:%02x:%02x",&tmpID[2], &tmpID[3], &tmpID[4], &tmpID[5]); 
        if (match_param == 4)
        {
            tmpID[0] = 0x86 ; tmpID[1] = 0x51 ; 
            for(int i = 0 ;i < 6 ; i++)
            {
                mac[i] = tmpID[i];
            }
            extern void write_mac_address(uint8_t * mac);
            write_mac_address(mac);   //存mac地址
            //os_reboot();                //重启
            success = 1;
        }  
    } 
    else
    {
        match_param = 0; 
        char * s1 = NULL;  
        char * s2 = NULL; 
        int  point = 0;           
        s1 = strstr(buf, "[netconfig] ssid:");  //判断收到的指令是否包含“[netconfig] ssid:”
        s2 = strstr(buf, "passwd:");            //判断收到的指令是否包含“passwd:”
        if((s1 != NULL) && (s2 != NULL))        //2个指针都不为空
        {
            memset(ssid,0,sizeof(ssid));
            memset(passwd,0,sizeof(passwd));
            //获取passwd这个字符串的首地址
            for(int i = 0; i < len ; i++)
            {
                if((s1[i] == 'p') && (s1[i + 1] == 'a') && (s1[i + 2] == 's') && (s1[i + 3] == 's') && (s1[i + 4] == 'w') && (s1[i + 5] == 'd'))
                {
                    point = i;
                    LOG("point = %d,",point);
                    break;    //跳出for循环
                }
            }
            for(int i = 17; i < point - 1 ; i++)     //"[netconfig] ssid:"长度17，point - 1-1是因为前面还有一个空格
            {
                ssid[i - 17] = s1[i];                //"[netconfig] ssid:"和“passwd:”中间所有的字符就是账号
            }
            strcpy(passwd,&s2[7]);                   //“passwd:”长度为7，所以密码重第7位开始取值
            LOG("1 --- ssid:%s, passwd:%s,",ssid, passwd);

            match_param = 2;    //执行下面函数
        }
        if (match_param == 2) 
        {
            LOG("2 --- ssid:%s, passwd:%s", ssid, passwd);
            // normalConfig = 1;
            // config_success = 1;
            save_ssid_password(1 , ssid , passwd);    //保存帐号密码  
            //os_reboot();                //重启
            success = 2; 
        }
        else
        {
            // LOG("factory test");
            // memset(mServerip, 0, sizeof(mServerip));
            // match_param = sscanf(buf, "[FactoryMode] ssid:%s passwd:%s serverip:%s", ssid, passwd, mServerip);
            // if (match_param == 3) {
            //     LOG("%s --- ssid:%s, passwd:%s, serverip:%s", buf, ssid, passwd, mServerip);
            //     success = 3;
            // }  
            if(strstr(buf, "VSON-FAC") != NULL)     
            {
                LOG("factory test");
                if(buf[8] == 0xAA)
                {
                    uint8_t reply[30];
                    reply[0] = 'V';
                    reply[1] = 'S';
                    reply[2] = 'O';
                    reply[3] = 'N';
                    reply[4] = '-';
                    reply[5] = 'F';
                    reply[6] = 'A';
                    reply[7] = 'C';


                    reply[8] = 0xAA;
                    reply[9] = temperature1 / 256;
                    reply[10] = temperature1 % 256;
                    reply[11] = temperature2 / 256;
                    reply[12] = temperature2 % 256;
                    reply[13] = 0;
                    reply[14] = 0;
                    reply[15] = is_lack_water;
                    reply[16] = system_parameter.onoff;
                    reply[17] = system_parameter.set_temperature;
                    reply[18] = MAX_POWER / 256;
                    reply[19] = MAX_POWER % 256;

                    printf("\n sned data:");
                    for(int i = 0; i < 20 ; i++)
                    {
                        printf("%02x",reply[i]);
                    }
                    printf("\n");

                    ssize_t write_len = 20;
                    char *write_ptr = reply;
                    while (write_len > 0) {
                        ssize_t write_cnt = write(clientfd, write_ptr, write_len);
                        if (write_cnt > 0) {
                            write_len -= write_cnt;
                            write_ptr += write_cnt;
                        } else {
                                break;
                        }
                    }
                }
                else if(buf[8] == 0xAB)
                {
                    system_parameter.onoff = buf[9];
                    printf("\n sned data:");
                    for(int i = 0; i < 10 ; i++)
                    {
                        printf("%02x ",buf[i]);
                    }
                    printf("\n");

                     ssize_t write_len = 10;
                    char *write_ptr = buf;
                    while (write_len > 0) {
                        ssize_t write_cnt = write(clientfd, write_ptr, write_len);
                        if (write_cnt > 0) {
                            write_len -= write_cnt;
                            write_ptr += write_cnt;
                        } else {
                                break;
                        }
                    }
                }
                else if(buf[8] == 0xAC)
                {
                    system_parameter.set_temperature = buf[9];
                    printf("\n sned data:");
                    for(int i = 0; i < 10 ; i++)
                    {
                        printf("%02x ",buf[i]);
                    }
                    printf("\n");

                     ssize_t write_len = 10;
                    char *write_ptr = buf;
                    while (write_len > 0) {
                        ssize_t write_cnt = write(clientfd, write_ptr, write_len);
                        if (write_cnt > 0) {
                            write_len -= write_cnt;
                            write_ptr += write_cnt;
                        } else {
                                break;
                        }
                    }
                } 
                else if(buf[8] == 0xAD)
                {
                    MAX_POWER = (uint32_t)buf[11] * 256 + buf[12];

                    printf("\n sned data:");
                    for(int i = 0; i < 13 ; i++)
                    {
                        printf("%02x ",buf[i]);
                    }
                    printf("\n");

                    LOG("MAXPower = %d" , MAX_POWER);
                    Writemaxpower();      //保存
                     ssize_t write_len = 13;
                    char *write_ptr = buf;
                    while (write_len > 0) {
                        ssize_t write_cnt = write(clientfd, write_ptr, write_len);
                        if (write_cnt > 0) {
                            write_len -= write_cnt;
                            write_ptr += write_cnt;
                        } else {
                                break;
                        }
                    }
                }     
            }     
            else
            {
                unsigned int power[4];
                match_param = sscanf(buf, "[vson power] %02x:%02x:%02x:%02x",&power[0], &power[1],&power[2],&power[3]);
                if(match_param == 4)
                {
                    LOG("power1234 = %d %d %d %d",power[0],power[1],power[2],power[3]);
                    unsigned int maxpower = (power[0] << 24) + (power[1] << 16) + (power[2] << 8) + power[3];
                    LOG("MAXPower = %d" , maxpower);
                    
                    MAX_POWER = maxpower;
                    Writemaxpower();      //保存
    //                success = 2; 
                    sprintf(buf, "[vson power] %02x:%02x:%02x:%02x", power[0], power[1],
                            power[2], power[3]);
                    LOG("%s", buf);
                    ssize_t write_len = strlen(buf);
                    char *write_ptr = buf;
                    while (write_len > 0) {
                        ssize_t write_cnt = write(clientfd, write_ptr, write_len);
                        if (write_cnt > 0) {
                            write_len -= write_cnt;
                            write_ptr += write_cnt;
                        } else {
                            return;
                        }
                    }
                }
            }
        }
        
    } 
    if(success)
    {
        sprintf(buf, "[netid][%02X:%02X:%02X:%02X:%02X:%02X]", mac[0], mac[1],
                mac[2], mac[3], mac[4], mac[5]);
        LOG("%s", buf);
        ssize_t write_len = strlen(buf) + 1;
        char *write_ptr = buf;
        while (write_len > 0) {
            ssize_t write_cnt = write(clientfd, write_ptr, write_len);
            if (write_cnt > 0) {
                write_len -= write_cnt;
                write_ptr += write_cnt;
            } else {
                    break;
            }
        }
        if(success == 1)
        {
            aos_msleep(3000);  
            os_reboot();                //重启
        }
        else if(success == 2)
        {
            extern void vson_stop_awss();
            vson_stop_awss();           
        }
        else if(success == 3)
        {
            // //extern void factory_test(char flag , char * ssid , char * password);
            // save_ssid_password(1 , ssid , passwd);
            // extern void vson_stop_awss();
            // vson_stop_awss();  
            // success = 0;
            // save_ssid_password(0 , ssid , passwd);   //删除帐号密码
            // VSON_Factory_flag = 1;
            // aos_task_new_ext(&task_app_client_process, "task_app_client_process", app_client_process, NULL, 2048, AOS_DEFAULT_APP_PRI);            
        }
        success = 0;
    }
}


#define MAX_INPUT_CHAR 256
void communication(int fd)
{
    uint8_t input[MAX_INPUT_CHAR];
 
    while (1) {
        memset(input, 0, MAX_INPUT_CHAR);
        //等待client发送消息
        int len = read(fd, input, sizeof(input));
        if(len > 0)
        {
            g_rxtx_need_restart = 0;
            printf("Receive from client: %s\n", input);
            rec_process(input,len);
        }
        else  //数据异常
        {
            g_rxtx_need_restart = 1;
            close(clientfd);
            close(serverfd);
            printf("rec data error\n");
            return;
        }
    }
}
//创建socket
int create_tcp_server(int isCreatServer)
{
    if(isCreatServer)   //是否首次进入
    {
        memset(&servaddr, 0, sizeof(servaddr));
        memset(&clientaddr, 0, sizeof(clientaddr));
        serverfd = socket(AF_INET, SOCK_STREAM, 0);
    
        if (serverfd < 0) {
            printf("socket called failed!\n");
            return -1;
        }
        
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(10024);   //端口号
        //绑定server ip地址
        if ((bind(serverfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
            printf("socket bind failed...\n");
            return -1;
        }
    }
    //listen
    if ((listen(serverfd, 5)) != 0) {
        printf("Listen failed...\n");
        return -1;
    }

    len = sizeof(clientaddr);
    //等待设备连接
    clientfd = accept(serverfd, (struct sockaddr *)&clientaddr, &len);
    if (clientfd < 0) {
        printf("acccept failed!\n");
        close(serverfd);
        return -1;
    }
    return 0; 
}

void app_tcp_process(void *argv)
{
    LOG("task_app_tcp_process");

//    aos_msleep(1000);   //延迟1秒准备建立server

    while(1)
    {
        aos_msleep(1000);     //延迟2秒
        g_rxtx_need_restart = 0; 
        int socket_ret = create_tcp_server(1);   //创建一个socket
        if(socket_ret == -1)    //创建失败
        {
            LOG("create_tcp_server fail");
            continue;
        }
        else
        {
            LOG("create tcp socket succeed...");    
        }
        
        while(1)
        {
            if(g_rxtx_need_restart == 0)
            {
                communication(clientfd);    //读取数据
            }
            if(g_rxtx_need_restart)
            {
                LOG("tcp server error,some client leave,restart...");
                if (0 == create_tcp_server(1))    //创建成功
                {
                    g_rxtx_need_restart = 0;
                }
            }
            aos_msleep(10);
        }
    }
}


void creat_tcp_task()
{                                                                                                                                                                                                                                                                                                                        
    if(flag_creat)
    {
        LOG("task has creat");
        return;
    }
    flag_creat = 1;
    aos_task_new_ext(&task_app_tcp_process, "task_app_tcp_process", app_tcp_process, NULL, 2048, AOS_DEFAULT_APP_PRI);
}
//////////////////////////////////////////////////////////


/********************下面是客户端***************************/


////////////////////////////////////////////////////////////
#define TCP_SERVER_PORT 8932
//socket
static int server_socket = 0;                           //服务器socket
static struct sockaddr_in server_addr;                  //server地址
static struct sockaddr_in client_addr;                  //client地址
static unsigned int socklen = sizeof(client_addr);      //地址长度
static int connect_socket = 0;                          //连接socket
bool g_sta_need_restart = false;                       //异常后，重新连接标记    

void client_send_data();
//创建TCP client
int create_tcp_client()
{

    LOG("will connect tcp server ip : %s port:%d",
             mServerip, TCP_SERVER_PORT);
    //第一步：新建socket
    connect_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connect_socket < 0)
    {
        //打印报错信息
        LOG("create client fail");
        //新建失败后，关闭新建的socket，等待下次新建
        close(connect_socket);
        return -1;
    }
    //配置连接服务器信息
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(mServerip);
    LOG("connectting server...");
    //第二步：连接服务器
    if (connect(connect_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        //打印报错信息
        LOG("connect failed!");
        //连接失败后，关闭之前新建的socket，等待下次新建
        close(connect_socket);
        return -1;
    }

    send(connect_socket, "tcp client connected!!\r\n", strlen("tcp client connected!!\r\n"), 0);

    LOG("connect success!");
    return 0;
}
//TCP接收数据任务

void client_recv_data()
{
    int len = 0;            //长度
    char databuff[100];    //缓存
    uint8_t reply[20];
    while (1)
    {
        //清空缓存
        memset(databuff, 0x00, sizeof(databuff));
        //读取接收数据
        len = recv(connect_socket, databuff, sizeof(databuff), 0);
        g_sta_need_restart = false;
        if (len > 0)
        {
            LOG("recvData: %s", databuff);
            if(len == 2)
            {
                if(databuff[0] == 0xAA)
                {
                    reply[0] = 0xAA;
                    reply[1] = temperature1 / 256;
                    reply[2] = temperature1 % 256;
                    reply[3] = temperature2 / 256;
                    reply[4] = temperature2 % 256;
                    reply[5] = 0;
                    reply[6] = 0;
                    reply[7] = system_parameter.onoff;
                    reply[8] = system_parameter.set_temperature;
                    reply[9] = MAX_POWER / 256;
                    reply[10] = MAX_POWER % 256;
                    send(connect_socket, reply, 11, 0);
                }
                else if(databuff[0] == 0xAB)
                {
                    system_parameter.onoff = databuff[1];
                    send(connect_socket, databuff, 2, 0);
                }
                else if(databuff[0] == 0xAC)
                {
                    system_parameter.set_temperature = databuff[1];
                    send(connect_socket, databuff, 2, 0);
                }
            }
        }
        else
        {
            //服务器故障，标记重连
            g_sta_need_restart = true;
            
            break;
        }
    }
    close(connect_socket);
    close(server_socket);
    //标记重连
    g_sta_need_restart = true;
}
// //创建一个客户端
void app_client_process(void *argv)
{
    LOG("app_client_process has creat");
    while(1)
    {
        aos_msleep(1000);    //延迟1秒
        int socket_ret = create_tcp_client();   
        if (socket_ret == -1)
        {
            //建立失败
            LOG("create tcp socket error,stop...");
            continue;
        }
        else
        {
            client_recv_data();
            //client_send_data();
        }

        while (1)
        {
            aos_msleep(2000);   
            //重新建立client，流程和上面一样
            if (g_sta_need_restart)
            {
                aos_msleep(2000); 
                LOG("reStart create tcp client...");
                //建立client
                int socket_ret = create_tcp_client();

                if (socket_ret == -1)
                {
                    LOG("reStart create tcp socket error,stop...");
                    continue;
                }
                else
                {
                    LOG("reStart create tcp socket succeed...");
                    //重新建立完成，清除标记
                    g_sta_need_restart = false;
                    //建立tcp接收数据任务
                    //client_recv_data();
                    client_send_data();
                }
            }
        }
        aos_msleep(10); 
    }
}


// extern char factory_test_flag;
// extern uint32_t factory_test_can;
// extern uint32_t factory_test_adc;
void client_send_data()
{
    // uint8_t array[22];
    // uint32_t datatemp = 0;
    // uint8_t deviceID[6];
    // memcpy(deviceID , read_mac_address() , 6);   //复制地址
    // while(1)
    // {
    //     factory_test_flag = 1;
        
    //     array[0] = 0xAA;
    //     array[1] = 0x55;
    //     array[2] = deviceID[0];
    //     array[3] = deviceID[1];
    //     array[4] = deviceID[2];
    //     array[5] = deviceID[3];
    //     array[6] = deviceID[4];
    //     array[7] = deviceID[5];
    //     array[8] = (uint8_t)(datatemp >> 24);
    //     array[9] = (uint8_t)(datatemp >> 16);
    //     array[10] = (uint8_t)(datatemp >> 8);
    //     array[11] = datatemp;
        
    //     array[12] = (uint8_t)(factory_test_adc >> 24);
    //     array[13] = (uint8_t)(factory_test_adc >> 16);
    //     array[14] = (uint8_t)(factory_test_adc >> 8);
    //     array[15] = factory_test_adc;

    //     array[16] = (uint8_t)(factory_test_can >> 24);
    //     array[17] = (uint8_t)(factory_test_can >> 16);
    //     array[18] = (uint8_t)(factory_test_can >> 8);
    //     array[19] = factory_test_can;

    //     array[20] = 0x55;
    //     array[21] = 0xAA;
    //     datatemp++;
    //     if(send(connect_socket, array, sizeof(array), 0) == 22)
    //     {
    //         g_sta_need_restart = false;
    //         LOG("factory_test_adc = %d , factory_test_can = %d",factory_test_adc,factory_test_can);  
    //     }
    //     else
    //     {
    //         g_sta_need_restart = true;
    //         break;
    //     }    
    //     aos_msleep(2000); 
    // }
}