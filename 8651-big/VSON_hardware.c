#include <stdio.h>
#include <stdlib.h>
#include "VSON_common.h"
#include <hal/soc/gpio.h>
#include <hal/soc/pwm.h>
#include <hal/soc/uart.h>
#include <aos/log.h>
#include <hal/soc/timer.h>
#include <hal/soc/uart.h>
#include <hal/soc/wdg.h>

static gpio_dev_t io_key;   //按键
static gpio_dev_t io_led;   //LED


#define KEY_GPIO       17
#define LED_GPIO       15


static timer_dev_t tmr;  

extern uint32_t  MAX_POWER;

//初始化串口2
uart_dev_t my_uart;

static void uart_init(void)
{
    my_uart.port 			    = 1;
    my_uart.config.baud_rate    = 9600;
    my_uart.config.data_width   = DATA_WIDTH_8BIT;
    my_uart.config.parity	    = NO_PARITY;
    my_uart.config.stop_bits    = STOP_BITS_1;
    my_uart.config.flow_control = FLOW_CONTROL_DISABLED;

    hal_uart_init(&my_uart);
}


//GPIO初始化
void GPIO_Init()
{
    io_led.port = LED_GPIO;
    io_led.config = OUTPUT_PUSH_PULL;  //推挽输出
    hal_gpio_init(&io_led);

    io_key.port = KEY_GPIO;
    io_key.config = INPUT_PULL_UP;
    hal_gpio_init(&io_key);


//    hal_gpio_enable_irq(&io_ot,IRQ_TRIGGER_RISING_EDGE,input_Measure_Cb,(void*)0);
}


void  LED_Control(char i)
{
    if(i)
    {
        hal_gpio_output_high(&io_led);
    }
    else
    {
        hal_gpio_output_low(&io_led);
    }      
}

//读取GPIO的值
uint32_t read_KEY_1_value()
{
    uint32_t value;
    if(hal_gpio_input_get(&io_key, &value) == 0) //返回成功
    {
        return value;
    }
    return 0xff;   //读取失败，返回一个错误值
}

//看门狗初始化，5秒
static wdg_dev_t wdg;
static void dog_init()
{
    wdg.port = 0;
    wdg.config.timeout = 5000;

    hal_wdg_init(&wdg);
}

void feeddog()
{
    hal_wdg_reload(&wdg);
}
//硬件初始化
void Hardinit()
{
    GPIO_Init();
    dog_init();
    uart_init();
}


#define KEY_STAB_CNT            2
#define KEY_LONG_MAX_CNT        600         //6s

static unsigned int key1PressCnt_ = 0;
static unsigned char key1Process_ = 1;
extern void Key1_Press_Cb(char i);

void Key1_Scan(void)
{
    static uint8_t Flag = 0;
    if (read_KEY_1_value() != 0)    //按键没有按下
    {
        if(Flag == 1)   //按键松开之后检测计数值
        {                                
            Key1_Press_Cb(0);   //执行短按操作
            LOG("KEY1 short press");
            Flag = 0;            
        }
        key1PressCnt_ = 0;    //按下计数清零
        key1Process_ = 0;     //松开标志为0
    } 
    else 
    {
        if(key1Process_ == 0)         //此处限制是为了防止有人一直按，触发按键重复执行按键
        {
            key1PressCnt_ ++;          //按下开始计数
        }
        
        if (key1PressCnt_ >= KEY_STAB_CNT)   //连续2次检测到按键按下
        {
            Flag = 1;    //按键有效，但是先不处理
        }
        if(key1PressCnt_ >= KEY_LONG_MAX_CNT)               //超过最大值，即使按键没有松开也执行长按操作
        {
            Key1_Press_Cb(1);   //执行长按操作
            key1PressCnt_ = 0;
            key1Process_ = 1;   
            Flag = 0;  
            LOG("KEY1 long press 2");
        }
    }
}

void Key_Scan(void)
{
    Key1_Scan();
}