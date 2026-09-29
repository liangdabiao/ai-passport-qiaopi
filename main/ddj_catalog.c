// main/ddj_catalog.c —— 目录：按章号列出已收录的章，选中即开读。
//
// 一行两段：左边是章题（「道可道」），右边是章号（「第一章」）。之所以把章号
// 放在右侧的小字里而不是拼进主文字：24px 字下「第一章 道可道」已经 144px，
// 超过一行能给的 100px，拼起来只会两个都被打省略号。
//
// 表态（接了 / 还要想 / 没接上）不在这里显示 —— 那是「待参」那一页的职责，
// 挤进来只会让两边都看不清。
#include "ddj_catalog.h"

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

static void catalog_refresh_hint(void)
{
    if (!s_hint) return;

    char hint[64];
    const int total = ddj_chapter_total();
    const int count = ddj_chapter_count();
    if (count >= total) {
        snprintf(hint, sizeof(hint), "已收录全本 %d 章", total);
    } else {
        snprintf(hint, sizeof(hint), "已收录 %d/%d 章", count, total);
    }
    lv_label_set_text(s_hint, hint);
}

static void catalog_fill(void)
{
    const int count = ddj_chapter_count();

    // 章题在内容层是 const 字符串，直接用，不复制。
    for (int i = 0; i < count; i++) {
        const ddj_chapter_t *chapter = ddj_chapter_at(i);
        ddj_row_t *row = ddj_list_row(&s_list, i);
        if (!chapter || !row) continue;

        char label[DDJ_CHAPTER_LABEL_CAPACITY];
        if (!ddj_chapter_label(chapter->number, label, sizeof(label))) label[0] = '\0';

        ddj_row_update(row, chapter->title, label, DDJ_STATE_NORMAL);
    }
}

lv_obj_t *ddj_catalog_enter(void)
{
    const int count = ddj_chapter_count();
    if (count <= 0) return NULL;

    s_hint = NULL;
    ddj_list_clear(&s_list);

    lv_obj_t *card = NULL;
    lv_obj_t *scr = ddj_page_create(&card);
    if (!card) return scr;

    ddj_topbar_create(card, "目录");
    s_hint = ddj_hint_create(card, "");
    lv_obj_t *body = ddj_body_create(card);

    ddj_list_build(&s_list, body, count, DDJ_ROW_NOTE_W);
    for (int i = 0; i < count; i++) s_chapters[i] = i;
    catalog_fill();
    ddj_list_select(&s_list, 0);
    catalog_refresh_hint();
    return scr;
}

void ddj_catalog_leave(void)
{
    ddj_list_clear(&s_list);
    s_hint = NULL;
}

void ddj_catalog_key(ddj_key_t key)
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
