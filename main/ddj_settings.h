// main/ddj_settings.h —— 设置。
#pragma once

#include "lvgl.h"

#include "ddj_session.h"

lv_obj_t *ddj_settings_enter(void);
void ddj_settings_leave(void);
void ddj_settings_key(ddj_key_t key);
