#include "stm32f10x.h"                  // Device header
#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "CD.h" 
#include "Power.h"
#include "OLED.h"



    CD_State StateInit = NoDisc;                 //CD初始的状态
    int MusicNumber = 0;                        //初始化曲目
    CD_State LastState;                        //CD关机前的状态
    int LastMusicNumber = 1;                  // 初始化关机前的曲目为1

    OledMessages *Oledp = NULL;                //给Oled发送消息的指针
    PowerMsgs *Powerp = NULL;                 // 给Power发送消息的指针

    osThreadId CD_ID;                       //线程ID变量
    osPoolId CD_Pool;                      //内存池ID变量
    osMessageQId CD_Messages;             //消息队列ID变量
    osTimerId Timer;                     //定时器ID变量

    osThreadDef(CD_Thread,osPriorityNormal,1,0);         //定义线程结构体变量
    osPoolDef(CD_Pool, 2,CD_Msg_Type );                 //定义内存池结构体变量
    osMessageQDef(CD_MsgBox,1,&CD_Msg_Type);           //定义消息队列结构体变量
    osTimerDef(Timer,Timer_Callback);                 //定义定时器1结构体变量



    /* 保存LastState的函数 */
void SaveState(void) 
{
    LastState = StateInit;
    LastMusicNumber = MusicNumber;

}


    /* 恢复LastState的函数 */
void RestoreState(void) 
{
    StateInit = LastState;
    MusicNumber = LastMusicNumber;

}


    /* CD线程入口函数定义 */
void CD_Thread (void const *argument)
{
        osEvent CD_Event; //用于存储从消息队列中获取的事件信息
		CD_Msg_Type *Answer;//指向消息结构体的指针，用于处理从消息队列中接收到的消息
		CD_Msg CdRespond;//用于存储消息中的 CD_Res，即 CD 的响应消息。

    while(1)
    {
			CD_Event = osMessageGet(CD_Messages,osWaitForever);//获取事件消息
			if(CD_Event.status == osEventMessage)  //判断获取的事件是否为一个消息事件
		{
			Answer = (CD_Msg_Type *)CD_Event.value.p;//将消息值转换为指向消息结构体 CD_Msg_Type 的指针 Answer。
			CdRespond = Answer->CD_Res;//从消息结构体中提取响应消息 CD_Res 并存储在 CdRespond 中
			CdStateChange(CdRespond);//调用 CdStateChange 函数处理响应消息。
			osPoolFree(CD_Pool,Answer);//使用 osPoolFree 函数释放消息结构体所占用的内存池
		} 
        IWDG_ReloadCounter();
    }
}



    /* CD线程初始化 */
void CD_ThreadInit(void)
{
        CD_ID = osThreadCreate(osThread(CD_Thread),NULL); //创建CD线程对象
        CD_Pool = osPoolCreate(osPool(CD_Pool));  //创建CD内存池对象
        CD_Messages = osMessageCreate(osMessageQ(CD_MsgBox),NULL);//创建CD消息队列对象
        Timer = osTimerCreate(osTimer(Timer),osTimerOnce,NULL);//创建CDTimer1对象（3s）

        RestoreState();// 调用恢复上次保存函数
}


    /* 定时器回调函数 */
void Timer_Callback  (void const *arg)
{
        CD_Msg_Type *pointer1;
        pointer1 = osPoolCAlloc(CD_Pool);//从CD_Pool内存池中分配
        pointer1 -> CD_Res = CD_Wait_3s;//设置消息内容等待3s
        osMessagePut(CD_Messages,(uint32_t)pointer1,osWaitForever);//将消息放入CD_Messages消息队列中

}

/* CD给Power发送开机函数 */
void CD_Respond_ON(void) 
{
    Powerp = osPoolCAlloc(PowerPool);//从Power内存池中分配一个消息结构体
    Powerp ->PowerAnswer = PowerAnswer1;//设置消息内容为PowerAnswer1代表开机
    osMessagePut(PowerMessages,(uint32_t)Powerp,osWaitForever);//将消息放入消息队列中


}

/* CD给Power发送关机函数 */
void CD_Respond_OFF(void) 
{
    Powerp =osPoolCAlloc(PowerPool);//从Power内存池中分配一个消息结构体
    Powerp ->PowerAnswer = PowerAnswer2;//设置消息内容为PowerAnswer2代表关机
    osMessagePut(PowerMessages,(uint32_t)Powerp,osWaitForever);//将消息放入PowerMessage消息队列中

    SaveState();//调用保存当前状态函数
}

/* CD给OLED发送Load函数 */
void CD_Send_Load(void) 
{
        Oledp = osMailCAlloc(Mail,osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd=LOADING;//设置消息内容为LOADING
        osMailPut(Mail,Oledp);//将消息放入Mail消息队列中
        osTimerStart(Timer,3000);//开启定时器，设置时间为3s

}

/* CD给OLED发送Eject函数 */
void CD_Send_Eject(void) 
{
        Oledp = osMailCAlloc(Mail,osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd=EJECTING;//设置消息内容为EJECTING
        osMailPut(Mail,Oledp);//将消息放入Mail消息队列中
        osTimerStart(Timer,3000);//开启定时器，设置时间为3s

}

/* CD给OLED发送Play函数 */
void CD_Send_Play(void) 
{
        Oledp = osMailCAlloc(Mail,osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd=PLAY;//设置消息内容为PLAY
        Oledp->Music_Num=MusicNumber;//设置Music 曲目
        osMailPut(Mail,Oledp);//将消息放入Mail消息队列中

}

/* CD给OLED发送PAUSE函数 */
void CD_Send_Pause(void) 
{
        Oledp = osMailCAlloc(Mail,osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd=PAUSE;//设置消息内容为PAUSE
        Oledp->Music_Num=MusicNumber;//设置Music 曲目
        osMailPut(Mail,Oledp);//将消息放入Mail消息队列中

}

/* CD给OLED发送STOP函数 */
void CD_Send_Stop(void)
{
   Oledp = osMailCAlloc(Mail, osWaitForever);//从Mail内存池中分配一个消息结构体
   Oledp->OledResCd = STOP;//设置消息内容为STOP
   osMailPut(Mail, Oledp);//将消息放入Mail消息队列中

}

/* CD给OLED发送NO_DISC函数 */
void CD_Send_NoDisc(void)
{
	Oledp = osMailCAlloc(Mail, osWaitForever);//从Mail内存池中分配一个消息结构体
   Oledp->OledResCd = NO_DISC;//设置消息内容为NO_DISC
   osMailPut(Mail, Oledp);//将消息放入Mail消息队列中

}

/* CD给OLED发送连续上一曲函数 */
void CD_Send_FastPreviousing(void)
{
    int Num = 100; //本次模拟共100首歌
    int i;
    for (i = 0;i < Num;i++)
    {
        MusicNumber--;
			  osDelay(500);  //延迟0.5s
        if (MusicNumber < 1)
        {
            MusicNumber = 100;
        }

        Oledp = osMailCAlloc(Mail, osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd = PREVIOUS;//设置消息内容为PREVIOUS
        Oledp->Music_Num = MusicNumber;//设置Music曲目
        osMailPut(Mail, Oledp);//将消息放入Mail消息队列中
				if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == 1) //当按键为高电平时
				break;				  		
    }
		Oledp->OledResCd = PAUSE;
}

/* CD给OLED发送连续下一曲消息 */
void CD_Send_FastNexting(void)
{
    int Num = 100;//本次模拟共100首歌
    int i;
    for (i = 0;i < Num;i++)
    {	
        MusicNumber++;
			  osDelay(500);//延迟0.5s
        if (MusicNumber > 100)
        {
            MusicNumber = 1;
        }

        Oledp = osMailCAlloc(Mail, osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd = NEXT;//设置消息内容为NEXT
        Oledp->Music_Num = MusicNumber;//设置Music曲目
        osMailPut(Mail, Oledp);	//将消息放入Mail消息队列中
        if(GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == 1)//当按键为高电平时
				break;				
    }
		Oledp->OledResCd = PAUSE; //设置消息内容为PAUSE

}

/* CD给OLED发送上一曲函数 */
void CD_Send_Previous(void)
{
        MusicNumber--;
        if(MusicNumber < 1)
        {
                MusicNumber=100;
        }
        Oledp = osMailCAlloc(Mail,osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd=PLAY;//设置消息内容为PLAY
        Oledp->Music_Num=MusicNumber;//设置Music曲目
        osMailPut(Mail,Oledp);//将消息放入Mail消息队列中
}

/* CD给OLED发送下一曲消息 */
void CD_Send_Next(void)
{
        MusicNumber++;
        if(MusicNumber >100)
        {
                MusicNumber=001;
        }
        Oledp = osMailCAlloc(Mail,osWaitForever);//从Mail内存池中分配一个消息结构体
        Oledp->OledResCd=PLAY;//设置消息内容为PLAY
        Oledp->Music_Num=MusicNumber;//设置Music曲目
        osMailPut(Mail,Oledp);//将消息放入Mail消息队列中
}

/* 状态迁移表 */
State_Transition Arr[][4] =
{
 {  NoDisc,           CD_Power_ON,        NoDisc,            CD_Respond_ON   },
 {  NoDisc,           CD_Power_OFF,       NoDisc,            CD_Respond_OFF  },
 {  NoDisc,           CD_Load_Eject,      Load,              CD_Send_Load    },
 {  NoDisc,           CD_Play_Pause,      NoDisc,            CD_Send_NoDisc  },
 {  NoDisc,           CD_Previous,        NoDisc,            CD_Send_NoDisc  },
 {  NoDisc,           CD_Next,            NoDisc,            CD_Send_NoDisc  },

 {  Disc,             CD_Power_ON,        Stop,              CD_Send_Stop    },
 {  Disc,             CD_Power_OFF,       NoDisc,            CD_Respond_OFF  },
 {  Disc,             CD_Load_Eject,      Eject,             CD_Send_Eject   },

 {  Load,             CD_Power_OFF,       NoDisc,            CD_Respond_OFF  }, 
 {  Load,             CD_Load_Eject,      Eject,             CD_Send_Eject   },
 {  Load,             CD_Wait_3s,         Stop,              CD_Send_Stop    },

 {  Eject,            CD_Power_OFF,       Disc,              CD_Respond_OFF  }, 
 {  Eject,            CD_Load_Eject,      Load,              CD_Send_Stop    }, 
 {  Eject,            CD_Wait_3s,         NoDisc,            CD_Send_NoDisc  },

 {  Stop,             CD_Power_OFF,       Disc,              CD_Respond_OFF  },
 {  Stop,             CD_Load_Eject,      Eject,             CD_Send_Eject   },
 {  Stop,             CD_Play_Pause,      Play,              CD_Send_Play    },
 {  Stop,             CD_Previous,        Play,              CD_Send_Previous},
 {  Stop,             CD_Next,            Play,              CD_Send_Next    },

 {  Play,             CD_Power_OFF,       Disc,              CD_Respond_OFF  },
 {  Play,             CD_Play_Pause,      Pause,             CD_Send_Pause   },
 {  Play,             CD_Previous,        Play,              CD_Send_Previous},
 {  Play,             CD_Next,            Play,              CD_Send_Next    },
 {  Play,             CD_Load_Eject,      Eject,             CD_Send_Eject   },

 {  Pause,            CD_Power_OFF,       Disc,              CD_Respond_OFF  },
 {  Pause,            CD_Load_Eject,      Eject,             CD_Send_Eject   }, 
 {  Pause,            CD_Play_Pause,      Play,              CD_Send_Play    },
 {  Pause,            CD_Previous,        Previous,   CD_Send_FastPreviousing},
 {  Pause,            CD_Next,            Next,       CD_Send_FastNexting    },

 {  Previous,         CD_Power_OFF,       Disc,              CD_Respond_OFF  },
 {  Previous,         CD_Load_Eject,      Eject,             CD_Send_Eject   },
 {  Previous,         CD_Play_Pause ,     Play,              CD_Send_Play    },
 {  Previous,         CD_Previous ,       Previous,   CD_Send_FastPreviousing},
 {  Previous,         CD_Next ,           Next,       CD_Send_FastNexting    },

 {  Next,             CD_Power_OFF,       Disc,               CD_Respond_OFF },
 {  Next,             CD_Load_Eject,      Eject,              CD_Send_Eject  },
 {  Next,             CD_Play_Pause ,     Play,               CD_Send_Play   },
 {  Next,             CD_Previous ,       Previous,   CD_Send_FastPreviousing},
 {  Next,             CD_Next ,           Next,       CD_Send_FastNexting    },

 };

 /* 检查当前状态 和 接收到KEY的消息 */
int CheckTransition(CD_State s, CD_Msg m)  
{
    int ret = -1; // 返回值初始化为-1 表示未找到
    int flag = -1; // 标志变量初始化为-1 表示未找到
    int i;

    for (i = 0; i < sizeof(Arr) / sizeof(Arr[0]); i++)
    {
        //数组中的状态与传入的状态是否相等 并且 //数组中的消息与传入的消息是否相等
        if (Arr[i][0].Current == s && Arr[i][0].Msg == m) 
        {
            flag = i;
            ret = flag;
            break;
        }
    }
    return ret;
}

void CdStateChange(CD_Msg m)
{
    int index = CheckTransition(StateInit, m);// 检查当前状态和消息的转移情况

    if (index != -1)
    {
         // 执行状态转移函数
        Arr[index][0].fun();
        // 更新当前状态为下一个状态 
        StateInit = Arr[index][0].Next;
    }
    else
    {
        index = 0;
    }
}