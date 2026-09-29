// main/ddj_shelf.h —— 待参：表过态、但还没读通的章。
#pragma once

#include "lvgl.h"

#include "ddj_session.h"

lv_obj_t *ddj_shelf_enter(void);
void ddj_shelf_leave(void);
void ddj_shelf_key(ddj_key_t key);
