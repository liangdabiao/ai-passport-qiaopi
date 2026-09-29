// main/ddj_session.h —— 一次日课的四层推进：原文 → 点拨 → 参究 → 存档。
//
// 硬件只有三个键（上/下/确定，共用一个 ADC 分压引脚），语义一套走天下：
//   上 / 下 短按   在当前层里移动
//   确定    短按   往下走一层（读到最后一屏/条/项就换层）
//   确定    长按   退回上一层（在第一屏按就等于不读了，退出）
//
// 这一层刻意只算「现在在哪、按下去该到哪」，不碰 LVGL 也不碰内容表：
// 章数、屏数、条数、选项数都由调用方传进来，所以它能在电脑上完整测
// （tests/test_ddj_session.c）。
#pragma once

#include "ddj_progress.h"

typedef enum {
    DDJ_KEY_UP = 0,
    DDJ_KEY_DOWN,
    DDJ_KEY_OK,
    DDJ_KEY_BACK,
} ddj_key_t;

typedef enum {
    DDJ_STAGE_READ = 0, /* 原文，一屏一句 */
    DDJ_STAGE_POINT,    /* 点拨，一条一屏 */
    DDJ_STAGE_PONDER,   /* 参究，一问三选 */
    DDJ_STAGE_DONE,     /* 已记下 */
} ddj_stage_t;

typedef enum {
    DDJ_ACT_NONE = 0,  /* 键被吃下了，界面按新状态重画即可 */
    DDJ_ACT_FINISHED,  /* 看完并存档，应保留表态并回首页 */
    DDJ_ACT_CANCEL,    /* 中途退出，不应记录任何表态 */
} ddj_act_t;

typedef struct {
    int chapter;         /* 0 基章下标 */
    ddj_stage_t stage;   /* 当前在第几层 */
    int passage;         /* 当前第几屏原文 */
    int point;           /* 当前第几条点拨 */
    int cursor;          /* 参究光标，0 基 */
    ddj_slot_t previous; /* 进本章之前，这一章上次的表态 */
} ddj_session_t;

/* 开一次日课。previous 传这一章上次的表态（没有就 DDJ_SLOT_NONE）。 */
void ddj_session_begin(ddj_session_t *session, int chapter, ddj_slot_t previous);

/* 处理一次按键。屏数/条数/选项数由调用方给出，小于 1 会被夹到 1。 */
ddj_act_t ddj_session_key(ddj_session_t *session, ddj_key_t key,
                          int passage_count, int point_count, int option_count);

/* 当前选中的槽位，只在 PONDER / DONE 两层有意义。 */
ddj_slot_t ddj_session_slot(const ddj_session_t *session);
