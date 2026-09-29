// main/ddj_app.c —— 见 ddj_app.h。
//
// 切页顺序有个坑要说清楚：LVGL 删除「当前活动屏幕」会把显示屏的 act_scr 置空，
// 因此这里一律「先建新页 -> 切过去 -> 再删旧屏」，绝不先删旧的。
// 另外 leave() 在 enter() 之前调用，页面模块的 enter() 会把自己的缓存指针整套
// 重建，所以旧的 leave() 清不到新页的状态。
#include "ddj_app.h"

#include <stddef.h>

#include "bsp_display.h"
#include "ddj_catalog.h"
#include "ddj_daily.h"
#include "ddj_home.h"
#include "ddj_settings.h"
#include "ddj_shelf.h"

typedef enum {
    DDJ_PAGE_NONE = 0,
    DDJ_PAGE_HOME,
    DDJ_PAGE_DAILY,
    DDJ_PAGE_CATALOG,
    DDJ_PAGE_SHELF,
    DDJ_PAGE_SETTINGS,
} ddj_page_t;

static ddj_page_t s_page;
static lv_obj_t *s_screen;

// 停掉当前页：清掉控件指针，但不碰屏幕对象本身。
static void stop_current(void)
{
    switch (s_page) {
        case DDJ_PAGE_HOME:     ddj_home_leave(); break;
        case DDJ_PAGE_DAILY:    ddj_daily_leave(); break;
        case DDJ_PAGE_CATALOG:  ddj_catalog_leave(); break;
        case DDJ_PAGE_SHELF:    ddj_shelf_leave(); break;
        case DDJ_PAGE_SETTINGS: ddj_settings_leave(); break;
        default: break;
    }
    s_page = DDJ_PAGE_NONE;
}

// 切到新页并回收旧屏。new_screen 为 NULL 表示新页没建起来，保持原状。
static void adopt(ddj_page_t page, lv_obj_t *new_screen)
{
    if (!new_screen) return;

    lv_screen_load(new_screen);
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = new_screen;
    s_page = page;
}

void ddj_app_start(void)
{
    adopt(DDJ_PAGE_HOME, ddj_home_enter());
}

void ddj_app_goto_home(void)
{
    stop_current();
    adopt(DDJ_PAGE_HOME, ddj_home_enter());
}

void ddj_app_goto_daily(int chapter)
{
    stop_current();
    lv_obj_t *scr = ddj_daily_enter(chapter);
    // 建不成（章号越界、这一章还没收录）就退回首页，不能把用户留在一个已经
    // leave() 过、按键没反应的死页面上。
    adopt(scr ? DDJ_PAGE_DAILY : DDJ_PAGE_HOME, scr ? scr : ddj_home_enter());
}

void ddj_app_goto_catalog(void)
{
    stop_current();
    lv_obj_t *scr = ddj_catalog_enter();
    adopt(scr ? DDJ_PAGE_CATALOG : DDJ_PAGE_HOME, scr ? scr : ddj_home_enter());
}

void ddj_app_goto_shelf(void)
{
    stop_current();
    lv_obj_t *scr = ddj_shelf_enter();
    adopt(scr ? DDJ_PAGE_SHELF : DDJ_PAGE_HOME, scr ? scr : ddj_home_enter());
}

void ddj_app_goto_settings(void)
{
    stop_current();
    lv_obj_t *scr = ddj_settings_enter();
    adopt(scr ? DDJ_PAGE_SETTINGS : DDJ_PAGE_HOME, scr ? scr : ddj_home_enter());
}

void ddj_app_key(ddj_key_t key)
{
    // 这里是按键任务上下文，不是 LVGL 任务：必须拿锁才能碰控件。
    // 拿不到锁（200 ms 超时）就丢掉这一次按键，绝不阻塞按键任务 ——
    // 界面卡住时用户还能长按返回，而不会连返回都按不动。
    if (!bsp_lvgl_lock(200)) return;

    switch (s_page) {
        case DDJ_PAGE_HOME:     ddj_home_key(key); break;
        case DDJ_PAGE_DAILY:    ddj_daily_key(key); break;
        case DDJ_PAGE_CATALOG:  ddj_catalog_key(key); break;
        case DDJ_PAGE_SHELF:    ddj_shelf_key(key); break;
        case DDJ_PAGE_SETTINGS: ddj_settings_key(key); break;
        default: break;
    }

    bsp_lvgl_unlock();
}
