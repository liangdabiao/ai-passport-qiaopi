// main/ddj_home.h —— 首页。
#pragma once

#include "lvgl.h"

#include "ddj_session.h"

lv_obj_t *ddj_home_enter(void);
void ddj_home_leave(void);
void ddj_home_key(ddj_key_t key);
