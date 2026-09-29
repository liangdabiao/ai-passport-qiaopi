// main/szj_lesson.h —— 接句闯关与错题重练(同一个页面,两种题库)。
#pragma once

#include "lvgl.h"

#include "szj_app.h"

// 第 lesson 课(0 基)。建屏失败返回 NULL。
lv_obj_t *szj_lesson_enter(int lesson);

// 错题重练:把答错过的句子重新排一遍。无错题可练时返回 NULL。
lv_obj_t *szj_review_enter(void);

void szj_lesson_leave(void);

void szj_lesson_key(szj_key_t key);
