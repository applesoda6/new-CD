#ifndef  __CD_H
#define  __CD_H
#include "stm32f10x.h"                  // Device header
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "Power.h"
#include "OLED.h"

//外部变量声明
extern osPoolId PowerPool;
extern osMessageQId PowerMessages; 
extern osMailQId Mail;
extern osTimerId Timer;

/* CD线程创建函数 */
void CD_Thread (void const *argument);
/* CD线程初始化函数 */
void CD_ThreadInit(void);
/* 定时器回调函数 */
void Timer_Callback  (void const *arg);

/* 保存和恢复函数 */
void SaveState(void);
void RestoreState(void);

/* CD向Power回复开关机函数 */
void CD_Respond_ON(void);
void CD_Respond_OFF(void);
/* CD向OLED发送CD状态函数 */
void CD_Send_Load(void);
void CD_Send_Eject(void);
void CD_Send_Play(void);
void CD_Send_Pause(void);
void CD_Send_Stop(void);
void CD_Send_NoDisc(void);
void CD_Send_FastPreviousing(void);
void CD_Send_FastNexting(void);
void CD_Send_Previous(void);
void CD_Send_Next(void);

/* CD 收到的消息 */
typedef  enum
    {
    CD_Power_ON = 0x10, //开机
    CD_Power_OFF = 0x12, // 关机
    CD_Load_Eject = 0x01, //加载和弹出
    CD_Play_Pause = 0x02, //播放和暂停
    CD_Previous = 0x03, //上一曲
    CD_Next = 0x04, //下一曲
    CD_Wait_3s = 0x05, //等待3s
}CD_Msg;   //CD 收到消息的枚举类型


typedef struct
    {
    CD_Msg CD_Res;
}CD_Msg_Type;



typedef enum {
	NoDisc, //无CD
	Disc,  //有CD 
	Load,  //加载
	Eject, //弹出
	Stop, //停止
	Play, //播放
	Pause,//暂停
	Previous, //上一曲
	Next //下一曲

}CD_State;//CD机状态的枚举类型

typedef struct {
    CD_State Current;//当前状态
    CD_Msg Msg;  //接收到的消息
    CD_State Next; //下一状态
    void (*fun)(); //执行函数
} State_Transition; //状态迁移


void CdStateChange(CD_Msg m);
int CheckTransition(CD_State s, CD_Msg m);
#endif