// main/qpq_app.h —— 导航、应用状态与按键语义。
//
// 硬件只有三个键（上/下/确定，共用一个 ADC 分压引脚），所以全局只用一套语义：
//   上 / 下 短按   在当前页面里移动（选候选、选菜单项）
//   确定    短按   往下走一步（确认作答 / 下一题 / 进入某项）
//   确定    长按   退回上一层（答题中等于放弃本局回标题）
//
// 页面模块只实现「进入 / 离开 / 按键」三个动作，页面之间怎么跳由 qpq_app.c 统一
// 决定 —— 这样「返回该回哪儿」只有一处答案。四个页面的入口都在本文件里声明，
// 而不是各开一个头文件：它们只被导航层调用，集中一处更容易看出「一共有几页」。
#pragma once

#include "lvgl.h"

#include "qpq_progress.h"
#include "qpq_session.h"

// 建立并加载标题页。需在持有 LVGL 锁时调用。
void qpq_app_start(void);

// 处理一次按键。内部自行加解锁，可从任意任务调用。
void qpq_app_key(qpq_key_t key);

// ---- 页面跳转。都在持有 LVGL 锁的前提下调用（页面模块内部使用）----
void qpq_app_goto_title(void);
void qpq_app_goto_ask(void);
void qpq_app_goto_reveal(void);
void qpq_app_goto_summary(void);

// ---- 共享状态。页面模块通过这几个入口读写，不各自持有副本 ----
qpq_session_t *qpq_app_session(void);
const qpq_progress_t *qpq_app_progress(void);
bool qpq_app_audio_enabled(void);
void qpq_app_set_audio_enabled(bool enabled);
// 本局已用时间（秒）。从开始一局那一刻起算。
int qpq_app_elapsed_seconds(void);

// 页面在「开始一局」之后调用，记下起算时刻；结算页用它算用时。
void qpq_app_note_run_started(void);
// 结算页在进入本局结算时调用一次：把成绩并进存档并落盘，同时把本局出过的题
// 标为已见。重复调用会重复计数，所以调用方要自己保证只调一次。
void qpq_app_commit_run(void);

// 清空全部记录（已见位图与最好成绩），并落盘。会话持有的已见位图指向存档内部
// 的数组，复位是原地清零，所以指针仍然有效，不需要重建会话。
void qpq_app_reset_progress(void);

// ---- 页面模块（只由导航层与 qpq_app_key 调用）----
lv_obj_t *qpq_page_title_enter(void);
void qpq_page_title_leave(void);
void qpq_page_title_key(qpq_key_t key);

lv_obj_t *qpq_page_ask_enter(void);
void qpq_page_ask_leave(void);
void qpq_page_ask_key(qpq_key_t key);

lv_obj_t *qpq_page_reveal_enter(void);
void qpq_page_reveal_leave(void);
void qpq_page_reveal_key(qpq_key_t key);

lv_obj_t *qpq_page_summary_enter(void);
void qpq_page_summary_leave(void);
void qpq_page_summary_key(qpq_key_t key);
