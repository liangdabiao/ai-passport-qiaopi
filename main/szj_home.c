// main/szj_home.c —— 首页:一块竹简招牌 + 四个入口,底栏顺手报学习进度。
//
// 四个入口刻意只有四个:孩子用三个键翻菜单,项数越多越容易迷失。
// 「错题本」在没有错题时按确定会响一声拒绝音并留在原地,而不是进一个空页面。
#include "szj_home.h"

#include <stddef.h>
#include <stdio.h>

#include "szj_progress.h"
#include "szj_sound.h"
#include "szj_store.h"
#include "szj_ui.h"

#define HOME_ITEMS 4

static const char *const HOME_LABEL[HOME_ITEMS] = {
    "开始学习",
    "认字卡",
    "错题本",
    "设置",
};

static szj_row_t s_rows[HOME_ITEMS];
static lv_obj_t *s_hint;
static int s_sel;

// 首页版式(相对页面卡):
//   顶栏 0..36 / 招牌 44..110 / 菜单 118..280(4 行 h36 间隔 6)/ 底栏 284..310
// 行高 36 不是随手定的:行有 3px 边框,内容区剩 30px,而 24px 字库的
// line_height 是 29px。34 的话只剩 28px,文字会顶到边框上。
#define HOME_HERO_Y   44
#define HOME_HERO_H   66
#define HOME_MENU_Y   118
#define HOME_ROW_H    36
#define HOME_ROW_GAP  6

static void home_refresh(void)
{
    const szj_progress_t *progress = szj_store_progress();
    if (!progress) return;

    const int next = szj_progress_next_lesson(progress);
    const int completed = szj_progress_completed_lessons(progress);
    const int wrong = szj_progress_wrong_count(progress);

    char status[HOME_ITEMS][20];
    if (completed >= SZJ_LESSON_COUNT) {
        snprintf(status[0], sizeof(status[0]), "从头复习");
    } else {
        snprintf(status[0], sizeof(status[0]), "第 %d 课", next + 1);
    }
    snprintf(status[1], sizeof(status[1]), "第 %d 句", (int)progress->card_line + 1);
    if (wrong > 0) {
        snprintf(status[2], sizeof(status[2]), "%d 句", wrong);
    } else {
        snprintf(status[2], sizeof(status[2]), "暂无");
    }
    status[3][0] = '\0';

    for (int i = 0; i < HOME_ITEMS; i++) {
        szj_row_update(&s_rows[i], HOME_LABEL[i], status[i],
                       i == s_sel ? SZJ_STATE_SELECTED : SZJ_STATE_NORMAL);
    }

    if (s_hint) {
        char hint[64];
        snprintf(hint, sizeof(hint), "★ %d 星 · 已过关 %d/%d 课",
                 szj_progress_total_stars(progress), completed, SZJ_LESSON_COUNT);
        lv_label_set_text(s_hint, hint);
    }
}

static void home_activate(void)
{
    const szj_progress_t *progress = szj_store_progress();
    if (!progress) return;

    switch (s_sel) {
        case 0:
            szj_sound_play(SZJ_SOUND_ENTER);
            szj_app_goto_lesson(szj_progress_next_lesson(progress));
            break;
        case 1:
            szj_sound_play(SZJ_SOUND_ENTER);
            szj_app_goto_card();
            break;
        case 2:
            if (szj_progress_wrong_count(progress) == 0) {
                // 没东西可练:响一声拒绝音,留在首页,别把孩子带进空页面。
                szj_sound_play(SZJ_SOUND_WRONG);
                return;
            }
            szj_sound_play(SZJ_SOUND_ENTER);
            szj_app_goto_review();
            break;
        case 3:
            szj_sound_play(SZJ_SOUND_ENTER);
            szj_app_goto_settings();
            break;
        default:
            break;
    }
}

lv_obj_t *szj_home_enter(void)
{
    s_sel = 0;
    s_hint = NULL;
    for (int i = 0; i < HOME_ITEMS; i++) {
        s_rows[i] = (szj_row_t){0};
    }

    lv_obj_t *card = NULL;
    lv_obj_t *scr = szj_page_create(&card);
    if (!card) return scr;

    szj_topbar_create(card, "三字经");

    lv_obj_t *hero = szj_panel_create(card, SZJ_BODY_X, HOME_HERO_Y, SZJ_BODY_W,
                                      HOME_HERO_H, SZJ_C_GOLD_SOFT, SZJ_C_GOLD, 3);
    szj_scroll_emblem_create(hero, 8, 12);
    lv_obj_t *title = szj_label_create(hero, "三字经", &szj_font_32, SZJ_C_RED_DARK);
    lv_obj_set_pos(title, 66, 3);
    lv_obj_t *subtitle = szj_label_create(hero, "儿童启蒙游戏", &szj_font_16, SZJ_C_INK_SOFT);
    lv_obj_set_pos(subtitle, 66, 42);

    for (int i = 0; i < HOME_ITEMS; i++) {
        const int y = HOME_MENU_Y + i * (HOME_ROW_H + HOME_ROW_GAP);
        s_rows[i] = szj_row_create(card, SZJ_BODY_X, y, SZJ_BODY_W, HOME_ROW_H);
    }

    s_hint = szj_hint_create(card, "");
    home_refresh();
    return scr;
}

void szj_home_leave(void)
{
    s_hint = NULL;
    for (int i = 0; i < HOME_ITEMS; i++) {
        s_rows[i] = (szj_row_t){0};
    }
}

void szj_home_key(szj_key_t key)
{
    switch (key) {
        case SZJ_KEY_UP:
            if (s_sel == 0) return;
            s_sel--;
            szj_sound_play(SZJ_SOUND_MOVE);
            home_refresh();
            break;
        case SZJ_KEY_DOWN:
            if (s_sel + 1 >= HOME_ITEMS) return;
            s_sel++;
            szj_sound_play(SZJ_SOUND_MOVE);
            home_refresh();
            break;
        case SZJ_KEY_OK:
            home_activate();
            break;
        default:
            break;
    }
}
