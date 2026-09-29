// main/ddj_shelf.c —— 待参：上次读的时候说了「还要想」或「没接上」的章。
//
// 这一页不是「收藏」，而是「欠着的账」。道德经这个应用的价值不在读得快，在于
// 同一章过些天再读一遍会不会变 —— 这一页就是那个「再读一遍」的入口。
//
// 右侧小字显示的是上次的表态（「还要想」/「没接上」），因为在这张列表上
// 唯一有用的信息就是「我当时卡在哪儿」；章号在目录页里已经有了。
#include "ddj_shelf.h"

#include <stddef.h>
#include <stdio.h>

#include "ddj_app.h"
#include "ddj_chapter.h"
#include "ddj_progress.h"
#include "ddj_sound.h"
#include "ddj_store.h"
#include "ddj_ui.h"

static ddj_list_t s_list;
static int s_chapters[DDJ_LIST_CAPACITY];   // 行下标 -> 章下标
static lv_obj_t *s_hint;

static void shelf_fill(const ddj_progress_t *progress, int count)
{
    for (int i = 0; i < count; i++) {
        const int index = ddj_progress_pending_at(progress, ddj_chapter_count(), i);
        if (index < 0) break;
        s_chapters[i] = index;

        const ddj_chapter_t *chapter = ddj_chapter_at(index);
        ddj_row_t *row = ddj_list_row(&s_list, i);
        if (!chapter || !row) continue;

        const char *name = ddj_slot_name((int)ddj_progress_slot(progress, index));
        ddj_row_update(row, chapter->title, name, DDJ_STATE_NORMAL);
    }
}

lv_obj_t *ddj_shelf_enter(void)
{
    const ddj_progress_t *progress = ddj_store_progress();
    if (!progress) return NULL;

    const int count = ddj_progress_pending_count(progress, ddj_chapter_count());
    if (count <= 0) return NULL;

    s_hint = NULL;
    ddj_list_clear(&s_list);

    char title[24];
    snprintf(title, sizeof(title), "待参 %d 章", count);

    lv_obj_t *card = NULL;
    lv_obj_t *scr = ddj_page_create(&card);
    if (!card) return scr;

    ddj_topbar_create(card, title);
    s_hint = ddj_hint_create(card, "确定 重读这一章 · 长按 返回");
    lv_obj_t *body = ddj_body_create(card);

    ddj_list_build(&s_list, body, count, DDJ_ROW_NOTE_W);
    shelf_fill(progress, count);
    ddj_list_select(&s_list, 0);
    return scr;
}

void ddj_shelf_leave(void)
{
    ddj_list_clear(&s_list);
    s_hint = NULL;
}

void ddj_shelf_key(ddj_key_t key)
{
    switch (key) {
        case DDJ_KEY_UP:
            if (ddj_list_move(&s_list, -1)) ddj_sound_play(DDJ_SOUND_MOVE);
            break;
        case DDJ_KEY_DOWN:
            if (ddj_list_move(&s_list, 1)) ddj_sound_play(DDJ_SOUND_MOVE);
            break;
        case DDJ_KEY_OK: {
            if (s_list.count == 0) return;
            const int chapter = s_chapters[s_list.sel];
            ddj_sound_play(DDJ_SOUND_ENTER);
            ddj_app_goto_daily(chapter);
            break;
        }
        case DDJ_KEY_BACK:
            ddj_sound_play(DDJ_SOUND_BACK);
            ddj_app_goto_home();
            break;
        default:
            break;
    }
}
