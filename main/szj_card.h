// main/szj_card.h —— 认字卡:一句一卡,田字格里放大看。
#pragma once

#include "lvgl.h"

#include "szj_app.h"

lv_obj_t *szj_card_enter(void);

void szj_card_leave(void);

void szj_card_key(szj_key_t key);
