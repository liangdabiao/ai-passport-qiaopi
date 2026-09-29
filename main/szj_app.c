// main/szj_app.c —— 见 szj_app.h。
//
// 切页顺序有个坑要说清楚:LVGL 删除"当前活动屏幕"会把显示屏的 act_scr 置空,
// 因此这里一律"先建新页 → 切过去 → 再删旧屏",绝不先删旧的。
// 另外 leave() 在 enter() 之前调用,页面模块的 enter() 会把自己的缓存指针整套
// 重建,所以旧的 leave() 清不到新页的状态。
#include "szj_app.h"

#include <stddef.h>

#include "bsp_display.h"
#include "szj_card.h"
#include "szj_home.h"
#include "szj_lesson.h"
#include "szj_settings.h"

typedef enum {
    SZJ_PAGE_NONE = 0,
    SZJ_PAGE_HOME,
    SZJ_PAGE_LESSON,
    SZJ_PAGE_CARD,
    SZJ_PAGE_SETTINGS,
} szj_page_t;

static szj_page_t s_page;
static lv_obj_t *s_screen;

// 停掉当前页:取消定时器、清掉对象指针,但不碰屏幕对象本身。
static void stop_current(void)
{
    switch (s_page) {
        case SZJ_PAGE_HOME:     szj_home_leave(); break;
        case SZJ_PAGE_LESSON:   szj_lesson_leave(); break;
        case SZJ_PAGE_CARD:     szj_card_leave(); break;
        case SZJ_PAGE_SETTINGS: szj_settings_leave(); break;
        default: break;
    }
    s_page = SZJ_PAGE_NONE;
}

// 切到新页并回收旧屏。new_screen 为 NULL 表示新页没建起来,保持原状。
static void adopt(szj_page_t page, lv_obj_t *new_screen)
{
    if (!new_screen) return;

    lv_screen_load(new_screen);
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = new_screen;
    s_page = page;
}

void szj_app_start(void)
{
    adopt(SZJ_PAGE_HOME, szj_home_enter());
}

void szj_app_goto_home(void)
{
    stop_current();
    adopt(SZJ_PAGE_HOME, szj_home_enter());
}

void szj_app_goto_lesson(int lesson)
{
    stop_current();
    lv_obj_t *scr = szj_lesson_enter(lesson);
    // 建不成(课号越界、题目生成失败)就退回首页,不能把用户留在一个已经
    // leave() 过、按键没反应的死页面上。
    adopt(scr ? SZJ_PAGE_LESSON : SZJ_PAGE_HOME, scr ? scr : szj_home_enter());
}

void szj_app_goto_review(void)
{
    stop_current();
    lv_obj_t *scr = szj_review_enter();
    adopt(scr ? SZJ_PAGE_LESSON : SZJ_PAGE_HOME, scr ? scr : szj_home_enter());
}

void szj_app_goto_card(void)
{
    stop_current();
    lv_obj_t *scr = szj_card_enter();
    adopt(scr ? SZJ_PAGE_CARD : SZJ_PAGE_HOME, scr ? scr : szj_home_enter());
}

void szj_app_goto_settings(void)
{
    stop_current();
    lv_obj_t *scr = szj_settings_enter();
    adopt(scr ? SZJ_PAGE_SETTINGS : SZJ_PAGE_HOME, scr ? scr : szj_home_enter());
}

void szj_app_key(szj_key_t key)
{
    // 这里是按键任务上下文,不是 LVGL 任务:必须拿锁才能碰控件。
    // 拿不到锁(200 ms 超时)就丢掉这一次按键,绝不阻塞按键任务 ——
    // 界面卡住时用户还能长按返回,而不会连返回都按不动。
    if (!bsp_lvgl_lock(200)) return;

    switch (s_page) {
        case SZJ_PAGE_HOME:     szj_home_key(key); break;
        case SZJ_PAGE_LESSON:   szj_lesson_key(key); break;
        case SZJ_PAGE_CARD:     szj_card_key(key); break;
        case SZJ_PAGE_SETTINGS: szj_settings_key(key); break;
        default: break;
    }

    bsp_lvgl_unlock();
}
