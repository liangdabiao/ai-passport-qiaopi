// main/szj_home.h —— 首页(开始学习 / 认字卡 / 错题本 / 设置)。
#pragma once

#include "lvgl.h"

#include "szj_app.h"

// 建立并加载首页,返回逻辑屏幕根对象(由 szj_app 负责删除)。
lv_obj_t *szj_home_enter(void);

// 页面被切走前调用:清掉缓存的对象指针。
void szj_home_leave(void);

void szj_home_key(szj_key_t key);
