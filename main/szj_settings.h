// main/szj_settings.h —— 设置:重置学习进度、音效开关。
#pragma once

#include "lvgl.h"

#include "szj_app.h"

lv_obj_t *szj_settings_enter(void);

void szj_settings_leave(void);

void szj_settings_key(szj_key_t key);
