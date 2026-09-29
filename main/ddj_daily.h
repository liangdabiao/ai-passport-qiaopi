// main/ddj_daily.h —— 日课：原文 -> 点拨 -> 参究 -> 记下，四层推进。
#pragma once

#include "lvgl.h"

#include "ddj_session.h"

// chapter 是 0 基章下标；该章还没收录时返回 NULL。
lv_obj_t *ddj_daily_enter(int chapter);
void ddj_daily_leave(void);
void ddj_daily_key(ddj_key_t key);
