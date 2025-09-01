#ifndef __CD_H__
#define __CD_H__

#include "cmsis_os.h"
#include "stdint.h"

/* =========================
 * ͳһ��Ϣ�壨��ģ��Ψһ��ʽ��
 * ========================= */
typedef struct
{
    uint8_t  eventID;          /* �¼�ID�����շ�������¼��� */
    uint32_t scourceID;        /* ���ͷ�ģ��ID����ԭƴд��   */
    uint32_t targetID;         /* ���շ�ģ��ID               */
    uint32_t option[3];        /* ��չ�ֶΣ�����/�����ģ�    */
} MsgMail_t;

/* =========================
 * ģ��ID����Դ/Ŀ�꣩
 * ========================= */
enum
{
    MOD_POWER = 0x01,
    MOD_KEY = 0x02,
    MOD_CD = 0x03,
    MOD_OLED = 0x04
};

/* =========================
 * CD ��״̬
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
 * CD �ڲ��¼����� CD �Լ����ѣ�
 * ��ӳ���ԡ��ⲿ���ġ� EVT_*��
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
 * ��˭����˭���塱����CD ���յ��ⲿ�¼�
 * ��Щ���ǡ����˷��� CD�����¼���
 * ========================= */
#define EVT_PWR_ON                 0x10  /* Power ON ����   �� CD */
#define EVT_PWR_OFF                0x11  /* Power OFF ����  �� CD */

#define EVT_KEY_LOAD_EJECT         0x00  /* KEY0�̰���Load/Eject �� CD */
#define EVT_KEY_PREV_START         0x01  /* KEY?������ʼ Prev   �� CD */
#define EVT_KEY_PREV_STOP          0x02  /* KEY?�������� Prev   �� CD */
#define EVT_KEY_PLAY_PAUSE         0x03  /* KEY?�̰� Play/Pause �� CD */
#define EVT_KEY_NEXT_START         0x04  /* KEY?������ʼ Next   �� CD */
#define EVT_KEY_NEXT_STOP          0x05  /* KEY?�������� Next   �� CD */

 /* CD �ڲ���Ͷ�ݣ��� CD �Լ������Լ��� */
#define EVT_INT_WAIT_3S_TIMEOUT    0x40
#define EVT_INT_FAST_TICK_500MS    0x41

/* =========================
 * �������������ɸ�ģ�� .c ������
 * ========================= */
#ifdef __cplusplus
extern "C" {
#endif

    extern osMailQId g_mailq_power;  /* POWER �������䣨POWER.c ������ */
    extern osMailQId g_mailq_oled;   /* OLED  �������䣨OLED.c  ������ */
    extern osMailQId g_mailq_cd;     /* CD    �������䣨CD.c    ������ */

    /* =========================
     * CD ���� API
     * ========================= */
    void      CD_Init(void);
    void      CD_PostMessage(MsgMail_t* msg);
    CD_Mode_t CD_GetState(void);
    uint16_t  CD_GetTrackNo(void);

#ifdef __cplusplus
}
#endif

#endif /* __CD_H__ */
