// main/szj_app.h —— 三字经游戏的导航与按键语义。
//
// 硬件只有三个键(上/下/确定,共用一个 ADC 分压引脚),所以按键语义要能一套走天下:
//   上 / 下 短按   在当前页面里移动光标
//   确定    短按   确认(进入、作答)
//   确定    长按   返回上一级
// 页面模块只实现"进入/离开/按键"三个动作,页面之间怎么跳由 szj_app.c 统一决定。
#pragma once

#include "lvgl.h"

typedef enum {
    SZJ_KEY_UP = 0,
    SZJ_KEY_DOWN,
    SZJ_KEY_OK,
    SZJ_KEY_BACK,
} szj_key_t;

// 建立并加载首页。需在持有 LVGL 锁时调用。
void szj_app_start(void);

// 处理一次按键。内部自行加解锁,可从任意任务调用。
void szj_app_key(szj_key_t key);

// ---- 页面跳转。都在持有 LVGL 锁的前提下调用(页面模块内部使用) ----
void szj_app_goto_home(void);
void szj_app_goto_lesson(int lesson);
void szj_app_goto_review(void);
void szj_app_goto_card(void);
void szj_app_goto_settings(void);
