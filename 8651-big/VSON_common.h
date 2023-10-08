#ifndef __VSON_COMMON_H
#define __VSON_COMMON_H

extern char GetTimeState(void);


#define  num_historyRecord    10     //历史记录一共存储50条

typedef struct 
{
	uint8_t flag;
	uint8_t onoff;
	uint32_t set_temperature;
}system_parameter_t;

typedef struct{
	uint32_t  starttime;
	uint32_t  worktime;   
}historyRecord_t; 

extern system_parameter_t  system_parameter;
extern historyRecord_t historyRecord[num_historyRecord];
extern uint32_t  temperature_avr;

void myApp(void);
#define Standard_timestamp   1646894664 


typedef struct{
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t week;
}my_time_t;

void Hardinit();
void LED_Control(char i);
void ONOFF_Control(char i);
void LEDProcess();
void Key_Scan(void);
void Key1_Press_Cb(char i);
uint32_t return_current_time();
int Writesysappointment();
void UserConfigInit(void);
int Writerecord();
void MsgsSendToServer(uint8_t * temp,uint8_t len);
void set_first_report(char i);
void reportpumpallstate();
void onlinereportwaterpumprecord();
void offlinereportwaterpumprecord();
char GetServerStatus();
void bumpexec();
void creat_tcp_task();
my_time_t * time_conversion() ;

void creat_timer();
void write_buf(uint8_t index , uint8_t num);

extern void save_ssid_password(char flag , char * ssid , char * password);
void Control_Out_1(char i);
void Control_Out_2(char i);
void Control_Out_4(char i);
void FanCtl(int duty);
void lcd_display();
void ADC_Process();
void check_is_work();
void SaveHistoryRecord(uint32_t x,uint32_t y);
void report_current();
void reportall();
void reporthistory(uint32_t starttime,uint32_t worktime);
void is_report_his();
uint32_t read_iswater_value();
void POWER_ON_OFF(char i);
uint8_t read_current_power();
int Writemaxpower();
void reportsoftversion();
uint8_t read_current_power();
uint32_t return_temp_power();
void user_uart_rec();
void feeddog();

extern char     is_lack_water;
extern char      is_work;

#endif
