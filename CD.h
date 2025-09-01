#ifndef __CD_H__
#define __CD_H__

#include "cmsis_os.h"
#include "stdint.h"

/* =========================
 * 统一消息体（跨模块唯一格式）
 * ========================= */
typedef struct
{
    uint8_t  eventID;          /* 事件ID（接收方定义的事件） */
    uint32_t scourceID;        /* 发送方模块ID（按原拼写）   */
    uint32_t targetID;         /* 接收方模块ID               */
    uint32_t option[3];        /* 扩展字段（参数/上下文）    */
} MsgMail_t;

/* =========================
 * 模块ID（来源/目标）
 * ========================= */
enum
{
    MOD_POWER = 0x01,
    MOD_KEY = 0x02,
    MOD_CD = 0x03,
    MOD_OLED = 0x04
};

/* =========================
 * CD 主状态
 * ========================= */
typedef enum
{
    ST_POWER_OFF = 0,
    ST_NODISC,
    ST_LOAD,
    ST_EJECT,
    ST_STOP,
    ST_PLAY,
    ST_PAUSE,
    ST_PREV,
    ST_NEXT,
    ST_MAX
} CD_Mode_t;

/* =========================
 * CD 内部事件（仅 CD 自己消费）
 * （映射自“外部来的” EVT_*）
 * ========================= */
typedef enum
{
    EV_CD_POWER_ON = 0,
    EV_CD_POWER_OFF,
    EV_CD_LOAD_EJECT,
    EV_CD_WAIT_3S,
    EV_CD_PLAY_PAUSE,
    EV_CD_PREVIOUS,
    EV_CD_NEXT,
    EV_CD_TURN_0_5S,
    EV_CD_TURN_UP,
    EV_MAX
} CD_Event_t;

/* =========================
 * “谁接收谁定义”——CD 接收的外部事件
 * 这些宏是“别人发给 CD”的事件号
 * ========================= */
#define EVT_PWR_ON                 0x10  /* Power ON 请求   → CD */
#define EVT_PWR_OFF                0x11  /* Power OFF 请求  → CD */

#define EVT_KEY_LOAD_EJECT         0x00  /* KEY0短按：Load/Eject → CD */
#define EVT_KEY_PREV_START         0x01  /* KEY?长按开始 Prev   → CD */
#define EVT_KEY_PREV_STOP          0x02  /* KEY?长按结束 Prev   → CD */
#define EVT_KEY_PLAY_PAUSE         0x03  /* KEY?短按 Play/Pause → CD */
#define EVT_KEY_NEXT_START         0x04  /* KEY?长按开始 Next   → CD */
#define EVT_KEY_NEXT_STOP          0x05  /* KEY?长按结束 Next   → CD */

 /* CD 内部自投递（由 CD 自己发给自己） */
#define EVT_INT_WAIT_3S_TIMEOUT    0x40
#define EVT_INT_FAST_TICK_500MS    0x41

/* =========================
 * 对外邮箱句柄（由各模块 .c 创建）
 * ========================= */
#ifdef __cplusplus
extern "C" {
#endif

    extern osMailQId g_mailq_power;  /* POWER 接收邮箱（POWER.c 创建） */
    extern osMailQId g_mailq_oled;   /* OLED  接收邮箱（OLED.c  创建） */
    extern osMailQId g_mailq_cd;     /* CD    接收邮箱（CD.c    创建） */

    /* =========================
     * CD 对外 API
     * ========================= */
    void      CD_Init(void);
    void      CD_PostMessage(MsgMail_t* msg);
    CD_Mode_t CD_GetState(void);
    uint16_t  CD_GetTrackNo(void);

#ifdef __cplusplus
}
#endif

#endif /* __CD_H__ */
