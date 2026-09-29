// main/ddj_home.c —— 首页：四个入口，底栏报读经进度。
//
// 只有四个入口，而且第一个就是「今日一章」—— 打开机器按两下就能开始读，
// 不需要先找目录。三个键翻菜单，项数越多越容易迷失。
#include "ddj_home.h"

#include <stddef.h>
#include <stdio.h>

#include "ddj_app.h"
#include "ddj_chapter.h"
#include "ddj_progress.h"
#include "ddj_sound.h"
#include "ddj_store.h"
#include "ddj_ui.h"

#define HOME_ITEMS 4

static const char *const HOME_LABEL[HOME_ITEMS] = {
    "今日一章",
    "目录",
    "待参",
    "设置",
};

static ddj_row_t s_rows[HOME_ITEMS];
static lv_obj_t *s_hint;
static int s_sel;

// 首页版式（相对页面卡）：
//   顶栏 0..36 / 小字 38..58 / 菜单 64..(64+4x44+3x6) / 底栏 284..310
// 行高 44 是被字库逼出来的：行有 3px 边框，内容区剩 38px，而 24px 字库的
// line_height 是 29px —— 40 也够，但 44 让四行之间有余量，改字号不会顶破。
#define HOME_NOTE_Y    38
#define HOME_MENU_Y    64
#define HOME_ROW_H     DDJ_ROW_H
#define HOME_ROW_GAP   6

static void home_refresh(void)
{
    const ddj_progress_t *progress = ddj_store_progress();
    if (!progress) return;

    const int count = ddj_chapter_count();
    const int next = ddj_progress_next_chapter(progress, count);

    char notes[HOME_ITEMS][24];
    notes[0][0] = '\0';
    notes[1][0] = '\0';
    notes[2][0] = '\0';
    notes[3][0] = '\0';

    const ddj_chapter_t *chapter = next >= 0 ? ddj_chapter_at(next) : NULL;
    if (chapter) {
        if (!ddj_chapter_label(chapter->number, notes[0], sizeof(notes[0]))) {
            notes[0][0] = '\0';
        }
    }
    snprintf(notes[1], sizeof(notes[1]), "%d/%d", count, ddj_chapter_total());
    snprintf(notes[2], sizeof(notes[2]), "%d 章",
             ddj_progress_pending_count(progress, count));

    for (int i = 0; i < HOME_ITEMS; i++) {
        ddj_row_update(&s_rows[i], HOME_LABEL[i], notes[i],
                       i == s_sel ? DDJ_STATE_SELECTED : DDJ_STATE_NORMAL);
    }

    if (s_hint) {
        char hint[64];
        snprintf(hint, sizeof(hint), "已读 %d/%d 章 · 日课 %d 次",
                 ddj_progress_read_count(progress), count,
                 ddj_progress_sessions(progress));
        lv_label_set_text(s_hint, hint);
    }
}

static void home_activate(void)
{
    const ddj_progress_t *progress = ddj_store_progress();
    const int count = ddj_chapter_count();

    switch (s_sel) {
        case 0:
            if (!progress || count == 0) {
                // 一章都没有，别把用户带进空页面。
                ddj_sound_play(DDJ_SOUND_BLOCK);
                return;
            }
            ddj_sound_play(DDJ_SOUND_ENTER);
            ddj_app_goto_daily(ddj_progress_next_chapter(progress, count));
            break;
        case 1:
            ddj_sound_play(DDJ_SOUND_ENTER);
            ddj_app_goto_catalog();
            break;
        case 2:
            if (!progress || ddj_progress_pending_count(progress, count) == 0) {
                // 没有欠着的账：响一声拒绝，留在首页，而不是进一个空页面。
                ddj_sound_play(DDJ_SOUND_BLOCK);
                return;
            }
            ddj_sound_play(DDJ_SOUND_ENTER);
            ddj_app_goto_shelf();
            break;
        case 3:
            ddj_sound_play(DDJ_SOUND_ENTER);
            ddj_app_goto_settings();
            break;
        default:
            break;
    }
}

lv_obj_t *ddj_home_enter(void)
{
    s_sel = 0;
    s_hint = NULL;
    for (int i = 0; i < HOME_ITEMS; i++) {
        s_rows[i] = (ddj_row_t){0};
    }

    lv_obj_t *card = NULL;
    lv_obj_t *scr = ddj_page_create(&card);
    if (!card) return scr;

    ddj_topbar_create(card, "道德经");
    s_hint = ddj_hint_create(card, "");
    lv_obj_t *body = ddj_body_create(card);

    // 小字一行，说明这台机器是干什么的；不用大标题占地方。
    ddj_note_create(body, DDJ_BODY_X, HOME_NOTE_Y, DDJ_BODY_W, "日课 · 每天一章",
                    DDJ_C_MUTED);

    for (int i = 0; i < HOME_ITEMS; i++) {
        const int y = HOME_MENU_Y + i * (HOME_ROW_H + HOME_ROW_GAP);
        s_rows[i] = ddj_row_create(body, DDJ_BODY_X, y, DDJ_BODY_W, HOME_ROW_H,
                                   DDJ_ROW_NOTE_W);
    }

    home_refresh();
    return scr;
}

void ddj_home_leave(void)
{
    s_hint = NULL;
    for (int i = 0; i < HOME_ITEMS; i++) {
        s_rows[i] = (ddj_row_t){0};
    }
}

void ddj_home_key(ddj_key_t key)
{
    switch (key) {
        case DDJ_KEY_UP:
            if (s_sel == 0) return;
            s_sel--;
            ddj_sound_play(DDJ_SOUND_MOVE);
            home_refresh();
            break;
        case DDJ_KEY_DOWN:
            if (s_sel + 1 >= HOME_ITEMS) return;
            s_sel++;
            ddj_sound_play(DDJ_SOUND_MOVE);
            home_refresh();
            break;
        case DDJ_KEY_OK:
            home_activate();
            break;
        case DDJ_KEY_BACK:
            // 首页是根，长按返回什么也不做 —— 不做「退出应用」这种在
            // 一块不能关机的板子上没有意义的事。
            break;
        default:
            break;
    }
}
