// main/ddj_app.h —— 导航与按键语义。
//
// 硬件只有三个键（上/下/确定，共用一个 ADC 分压引脚），所以全局只用一套语义：
//   上 / 下 短按   在当前页面里移动（翻句、翻条、选态度、选条目）
//   确定    短按   往下走一步（下一句 / 下一条 / 记下表态 / 进入某页）
//   确定    长按   退回上一层；在日课的第一屏按就等于不读了
//
// 页面模块只实现「进入 / 离开 / 按键」三个动作，页面之间怎么跳由 ddj_app.c
// 统一决定 —— 这样「返回该回哪儿」只有一处答案。
#pragma once

#include "lvgl.h"

#include "ddj_session.h" // ddj_key_t 在这里定义，不重复造一份

// 建立并加载首页。需在持有 LVGL 锁时调用。
void ddj_app_start(void);

// 处理一次按键。内部自行加解锁，可从任意任务调用。
void ddj_app_key(ddj_key_t key);

// ---- 页面跳转。都在持有 LVGL 锁的前提下调用（页面模块内部使用）----
void ddj_app_goto_home(void);
void ddj_app_goto_daily(int chapter);
void ddj_app_goto_catalog(void);
void ddj_app_goto_shelf(void);
void ddj_app_goto_settings(void);
