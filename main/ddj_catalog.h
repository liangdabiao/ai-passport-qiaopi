// main/ddj_catalog.h —— 目录：已收录的章。
#pragma once

#include "lvgl.h"

#include "ddj_session.h"

lv_obj_t *ddj_catalog_enter(void);
void ddj_catalog_leave(void);
void ddj_catalog_key(ddj_key_t key);
