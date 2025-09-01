#include "CD.h"
#include "POWER.h"
#include "OLED.h"
#include <string.h>

/* ================== RTOS 资源：CD 接收邮箱 + 定时器 + 线程 ================== */
osMailQDef(CD_MAILQ, 24, MsgMail_t);
osMailQId g_mailq_cd = NULL;

static void cd_timer_wait3s_cb(void const* arg);
static void cd_timer_fasttick_cb(void const* arg);
osTimerDef(CD_TMR_WAIT3S, cd_timer_wait3s_cb);
osTimerDef(CD_TMR_FASTTICK, cd_timer_fasttick_cb);
static osTimerId s_tmr_wait3s = NULL;
static osTimerId s_tmr_fasttick = NULL;

static void cd_thread(void const* argument);
osThreadDef(cd_thread, osPriorityNormal, 1, 64);

/* ================== 运行时变量 ================== */
volatile CD_Mode_t g_cd_state = ST_POWER_OFF;
static  uint8_t    s_disc_present = 0;
static  uint16_t   s_track_no = 1;
static  uint8_t    s_fast_dir = 0;     /* 0=Prev,1=Next */

/* 外部邮箱（对方接收） */
extern osMailQId g_mailq_power;
extern osMailQId g_mailq_oled;

/* ================== 迁移表声明/实现 ================== */
typedef struct { CD_Mode_t next; void (*action)(const MsgMail_t* m); } CD_Trans_t;
#define T(NEXT,ACT) { (NEXT), (ACT) }

static void ACT_None(const MsgMail_t* m);
static void CD_Respond_ON(const MsgMail_t* m);
static void CD_Respond_OFF(const MsgMail_t* m);
static void ACT_Send_NoDisc(const MsgMail_t* m);
static void CD_Send_Load(const MsgMail_t* m);
static void CD_Send_Eject(const MsgMail_t* m);
static void CD_Send_Stop(const MsgMail_t* m);
static void CD_Send_NoDisc(const MsgMail_t* m);
static void CD_Send_Play(const MsgMail_t* m);
static void CD_Send_Pause(const MsgMail_t* m);
static void CD_Send_Previous(const MsgMail_t* m);
static void CD_Send_Next(const MsgMail_t* m);
static void CD_Send_FastPreviousing(const MsgMail_t* m);
static void CD_Send_FastNexting(const MsgMail_t* m);
static void CD_TimerOn(const MsgMail_t* m);
static void CD_TimerOff(const MsgMail_t* m);
static void CD_SaveState(const MsgMail_t* m);

static const CD_Trans_t g_fsm[EV_MAX][ST_MAX] =
{
    /* 事件\状态 | OFF                    NODISC                LOAD                    EJECT                   STOP                    PLAY                      PAUSE                     PREV                     NEXT */
    /* EV_CD_POWER_ON  */
    { T(ST_NODISC,CD_Respond_ON),  T(ST_NODISC,CD_SaveState), T(ST_LOAD, CD_SaveState), T(ST_EJECT,CD_SaveState),
      T(ST_STOP,  CD_SaveState),   T(ST_PLAY,  CD_SaveState), T(ST_PAUSE,CD_SaveState), T(ST_PREV, CD_SaveState),
      T(ST_NEXT,  CD_SaveState) },

		/* EV_CD_POWER_OFF */
    { T(ST_POWER_OFF, CD_Respond_OFF), T(ST_POWER_OFF,CD_Respond_OFF), T(ST_POWER_OFF,CD_Respond_OFF), T(ST_POWER_OFF,CD_Respond_OFF),
      T(ST_POWER_OFF, CD_Respond_OFF), T(ST_POWER_OFF,CD_Respond_OFF), T(ST_POWER_OFF,CD_Respond_OFF), T(ST_POWER_OFF,CD_Respond_OFF),
      T(ST_POWER_OFF, CD_Respond_OFF) },

    /* EV_CD_LOAD_EJECT */
    { T(ST_POWER_OFF, ACT_None),     T(ST_LOAD, CD_Send_Load),  T(ST_LOAD, ACT_None),      T(ST_EJECT,ACT_None),
      T(ST_EJECT,     CD_Send_Eject), T(ST_EJECT,CD_Send_Eject), T(ST_EJECT,CD_Send_Eject), T(ST_EJECT,CD_Send_Eject),
      T(ST_EJECT,     CD_Send_Eject) },

    /* EV_CD_WAIT_3S */
    { T(ST_POWER_OFF, ACT_None),     T(ST_NODISC,ACT_None),      T(ST_STOP, CD_Send_Stop), T(ST_NODISC,CD_Send_NoDisc),
      T(ST_STOP,      ACT_None),     T(ST_PLAY, ACT_None),       T(ST_PAUSE,ACT_None),       T(ST_PREV, ACT_None),
      T(ST_NEXT,      ACT_None) },

    /* EV_CD_PLAY_PAUSE */
    { T(ST_POWER_OFF, ACT_None),     T(ST_NODISC,ACT_Send_NoDisc), T(ST_LOAD, ACT_None),    T(ST_EJECT,ACT_None),
      T(ST_PLAY,      CD_Send_Play),  T(ST_PAUSE,CD_Send_Pause), T(ST_PLAY, CD_Send_Play), T(ST_PREV, ACT_None),
      T(ST_NEXT,      ACT_None) },

    /* EV_CD_PREVIOUS */
    { T(ST_POWER_OFF, ACT_None),     T(ST_NODISC,ACT_Send_NoDisc), T(ST_LOAD, ACT_None),    T(ST_EJECT,ACT_None),
      T(ST_PREV,      CD_Send_Previous),  T(ST_PREV, CD_Send_Previous),  T(ST_PAUSE,CD_TimerOn), T(ST_PREV, CD_Send_Previous),
      T(ST_PREV,      CD_Send_Previous) },

    /* EV_CD_NEXT */
    { T(ST_POWER_OFF, ACT_None),     T(ST_NODISC,ACT_Send_NoDisc), T(ST_LOAD, ACT_None),    T(ST_EJECT,ACT_None),
      T(ST_NEXT,      CD_Send_Next),  T(ST_NEXT, CD_Send_Next),  T(ST_PAUSE,CD_TimerOn), T(ST_NEXT, CD_Send_Next),
      T(ST_NEXT,      CD_Send_Next) },

    /* EV_CD_TURN_0_5S */
    { T(ST_POWER_OFF, ACT_None),     T(ST_NODISC,ACT_None),      T(ST_LOAD, ACT_None),      T(ST_EJECT,ACT_None),
      T(ST_STOP,      ACT_None),     T(ST_PLAY, ACT_None),       T(ST_PAUSE,CD_Send_FastPreviousing/*方向里判*/),
      T(ST_PREV,      CD_Send_FastPreviousing), T(ST_NEXT, CD_Send_FastNexting) },

    /* EV_CD_TURN_UP */
    { T(ST_POWER_OFF, ACT_None),     T(ST_NODISC,ACT_None),      T(ST_LOAD, ACT_None),      T(ST_EJECT,ACT_None),
      T(ST_STOP,      CD_TimerOff), T(ST_PLAY, CD_TimerOff), T(ST_PAUSE,CD_TimerOff),
      T(ST_STOP,      CD_TimerOff), T(ST_STOP, CD_TimerOff) },
};

/* ================== 对外 API ================== */
void CD_Init(void)
{
    g_mailq_cd = osMailCreate(osMailQ(CD_MAILQ), NULL);
    s_tmr_wait3s = osTimerCreate(osTimer(CD_TMR_WAIT3S), osTimerOnce, NULL);
    s_tmr_fasttick = osTimerCreate(osTimer(CD_TMR_FASTTICK), osTimerPeriodic, NULL);
    osThreadCreate(osThread(cd_thread), NULL);
}

void CD_PostMessage(MsgMail_t* msg)
{
    if (!g_mailq_cd || !msg) return;
    osMailPut(g_mailq_cd, msg);
}

CD_Mode_t CD_GetState(void) { return g_cd_state; }
uint16_t  CD_GetTrackNo(void) { return s_track_no; }

/* ================== 消息→事件映射 ================== */
static CD_Event_t map_msg_to_event(const MsgMail_t* m)
{
    if (m->eventID == EVT_PWR_ON)                 return EV_CD_POWER_ON;
    if (m->eventID == EVT_PWR_OFF)                return EV_CD_POWER_OFF;
    if (m->eventID == EVT_KEY_LOAD_EJECT)         return EV_CD_LOAD_EJECT;
    if (m->eventID == EVT_INT_WAIT_3S_TIMEOUT)    return EV_CD_WAIT_3S;
    if (m->eventID == EVT_KEY_PLAY_PAUSE)         return EV_CD_PLAY_PAUSE;
    if (m->eventID == EVT_KEY_PREV_START) { s_fast_dir = 0; return EV_CD_PREVIOUS; }
    if (m->eventID == EVT_KEY_NEXT_START) { s_fast_dir = 1; return EV_CD_NEXT; }
    if (m->eventID == EVT_INT_FAST_TICK_500MS)    return EV_CD_TURN_0_5S;
    if (m->eventID == EVT_KEY_PREV_STOP ||
        m->eventID == EVT_KEY_NEXT_STOP)          return EV_CD_TURN_UP;
    return EV_MAX;
}

/* ================== 表驱动派发 ================== */
static void fsm_dispatch(CD_Event_t ev, const MsgMail_t* m)
{
    if (ev >= EV_MAX) return;
    {
        CD_Trans_t t = g_fsm[ev][g_cd_state];
        if (t.action) t.action(m);
        g_cd_state = t.next;
    }
}

/* ================== 线程入口 ================== */
static void cd_thread(void const* argument)
{
//    (void)argument;
//    for (;;) {
////        osEvent e = osMailGet(g_mailq_cd, osWaitForever);
//        if (e.status != osEventMail) continue;
//        {
//            MsgMail_t* m = (MsgMail_t*)e.value.p;
//            if (m && m->targetID == MOD_CD) {
//                CD_Event_t ev = map_msg_to_event(m);
//                fsm_dispatch(ev, m);
//            }
//            osMailFree(g_mailq_cd, m);
//        }
//    }
}

/* ================== 定时器回调（内部自投递） ================== */
static void cd_timer_wait3s_cb(void const* arg)
{
    (void)arg;
    MsgMail_t* m = (MsgMail_t*)osMailAlloc(g_mailq_cd, 0);
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->eventID = EVT_INT_WAIT_3S_TIMEOUT;
    m->scourceID = MOD_CD;
    m->targetID = MOD_CD;
    osMailPut(g_mailq_cd, m);
}

static void cd_timer_fasttick_cb(void const* arg)
{
    (void)arg;
    MsgMail_t* m = (MsgMail_t*)osMailAlloc(g_mailq_cd, 0);
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->eventID = EVT_INT_FAST_TICK_500MS;
    m->scourceID = MOD_CD;
    m->targetID = MOD_CD;
    osMailPut(g_mailq_cd, m);
}

/* ================== 发往 POWER / OLED 的工具 ================== */
static void cd_send_to_power(uint8_t evt)
{
    if (!g_mailq_power) return;
    MsgMail_t* m = (MsgMail_t*)osMailAlloc(g_mailq_power, 0);
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->eventID = evt;           /* POWER.h 定义：CD_ON_RECEIVE_OK/CD_OFF_RECEIVE_OK */
    m->scourceID = MOD_CD;
    m->targetID = MOD_POWER;
    osMailPut(g_mailq_power, m);
}

static void cd_send_to_oled(uint8_t evt, uint32_t a, uint32_t b, uint32_t c)
{
    if (!g_mailq_oled) return;
    MsgMail_t* mail = (MsgMail_t*)osMailAlloc(g_mailq_oled, 0);
    if (!mail) return;
    memset(mail, 0, sizeof(*mail));
    mail->eventID = evt;        /* OLED.h 定义：NO_DISC/PLAY/... */
    mail->scourceID = MOD_CD;
    mail->targetID = MOD_OLED;
    mail->option[0] = a; mail->option[1] = b; mail->option[2] = c;
    osMailPut(g_mailq_oled, mail);
}

static void cd_stop_all_timers(void)
{
    osTimerStop(s_tmr_wait3s);
    osTimerStop(s_tmr_fasttick);
}

/* ================== 动作实现 ================== */

/*  Act None  */
static void ACT_None(const MsgMail_t* m) { (void)m; }


/*  CD Respond POWER ON  */
static void CD_Respond_ON(const MsgMail_t* m)
{
    (void)m;
    cd_send_to_power(CD_ON_RECEIVE_OK);
    cd_send_to_oled(s_disc_present ? STOP : NO_DISC, (uint32_t)s_track_no, 0, 0);
}


/*  CD Respond POWER OFF  */
static void CD_Respond_OFF(const MsgMail_t* m)
{
    (void)m;
    cd_stop_all_timers();
    cd_send_to_power(CD_OFF_RECEIVE_OK);
}


/*  CD Send OLED NoDisc  */
static void ACT_Send_NoDisc(const MsgMail_t* m)
{
    (void)m;
    cd_send_to_oled(NO_DISC, 0, 0, 0);
}


/*  CD Send OLED Load*/
static void CD_Send_Load(const MsgMail_t* m)
{
    (void)m;
    s_disc_present = 1;
    cd_send_to_oled(LOADING, 0, 0, 0);
    osTimerStart(s_tmr_wait3s, 3000);
}


/*  CD Send OLED Eject  */
static void CD_Send_Eject(const MsgMail_t* m)
{
    (void)m;
    cd_send_to_oled(EJECTING, 0, 0, 0);
    osTimerStart(s_tmr_wait3s, 3000);
}


/*  CD Send OLED Stop  */
static void CD_Send_Stop(const MsgMail_t* m)
{
    (void)m;
    s_disc_present = 1;
    cd_send_to_oled(STOP, (uint32_t)s_track_no, 0, 0);
}


/*  CD Send OLED NoDisc  */
static void CD_Send_NoDisc(const MsgMail_t* m)
{
    (void)m;
    s_disc_present = 0;
    cd_send_to_oled(NO_DISC, 0, 0, 0);
}


/*  CD Send OLED Play  */
static void CD_Send_Play(const MsgMail_t* m)
{
    (void)m;
    if (!s_disc_present) { ACT_Send_NoDisc(NULL); return; }
    cd_send_to_oled(PLAY, (uint32_t)s_track_no, 0, 0);
}


/*  CD Send OLED Pause  */
static void CD_Send_Pause(const MsgMail_t* m)
{
    (void)m;
    if (!s_disc_present) { ACT_Send_NoDisc(NULL); return; }
    cd_send_to_oled(PAUSE, (uint32_t)s_track_no, 0, 0);
}


/*  CD Send OLED   */
static void step_prev_once(void)
{
    if (s_track_no <= 1) s_track_no = 100; else s_track_no--;
}
static void step_next_once(void)
{
    if (s_track_no >= 100) s_track_no = 1; else s_track_no++;
}


/*  CD Send OLED Previous  */
static void CD_Send_Previous(const MsgMail_t* m)
{
    (void)m;
    if (!s_disc_present) { ACT_Send_NoDisc(NULL); return; }
    step_prev_once();
    cd_send_to_oled(PREVIOUS, (uint32_t)s_track_no, 0, 0);
}


/*  CD Send OLED Next  */
static void CD_Send_Next(const MsgMail_t* m)
{
    (void)m;
    if (!s_disc_present) { ACT_Send_NoDisc(NULL); return; }
    step_next_once();
    cd_send_to_oled(NEXT, (uint32_t)s_track_no, 0, 0);
}


/*  CD Send OLED FastPreviousing  */
static void CD_Send_FastPreviousing(const MsgMail_t* m)
{
    (void)m;
    if (!s_disc_present) return;
    if (s_fast_dir == 0) step_prev_once();
    cd_send_to_oled(PREVIOUS, (uint32_t)s_track_no, 0, 0);
}


/*  CD Send OLED FastNexting  */
static void CD_Send_FastNexting(const MsgMail_t* m)
{
    (void)m;
    if (!s_disc_present) return;
    if (s_fast_dir == 1) step_next_once();
    cd_send_to_oled(NEXT, (uint32_t)s_track_no, 0, 0);
}


/*  CD TimerON*/
static void CD_TimerOn(const MsgMail_t* m)
{
    (void)m;
    osTimerStart(s_tmr_fasttick, 500);
}


/*  CD TimerOFF*/
static void CD_TimerOff(const MsgMail_t* m)
{
    (void)m;
    osTimerStop(s_tmr_fasttick);
}


/*  CD SaveState*/
static void CD_SaveState(const MsgMail_t* m)
{
    (void)m; /* 占位：需要时可做持久化 */
}
