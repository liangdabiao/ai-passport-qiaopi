// main/ddj_settings.c —— 设置：音效开关、重置进度。
//
// 重置不可逆，所以走两步：第一次按确定只把这一行变成朱砂警告，再按一次才真动手，
// 期间任何别的按键都算反悔。三个键的板子上，能清空几个月的读经记录的操作，
// 不能是一按就执行的。
#include "ddj_settings.h"

#include <stddef.h>
#include <stdio.h>

#include "ddj_app.h"
#include "ddj_chapter.h"
#include "ddj_sound.h"
#include "ddj_store.h"
#include "ddj_ui.h"

#define SETTINGS_ROW_Y    18
#define SETTINGS_ROW_GAP  8
#define SETTINGS_INFO_Y   126
#define SETTINGS_INFO_H   100

static ddj_row_t s_sound;
static ddj_row_t s_reset;
static lv_obj_t *s_hint;
static int s_sel;
static bool s_confirm;
static bool s_cleared;

static const char *const HINT_IDLE = "上/下 选择 · 确定 修改";
static const char *const HINT_CONFIRM = "再按一次确定 清空进度";

static void settings_refresh(void)
{
    ddj_row_update(&s_sound, "音效", ddj_sound_enabled() ? "开" : "关",
                   s_sel == 0 ? DDJ_STATE_SELECTED : DDJ_STATE_NORMAL);

    if (s_confirm) {
        // 确认态用选中样式，而不是「错误」色 —— 这个应用里没有对错这回事。
        ddj_row_update(&s_reset, "再按一次", "", DDJ_STATE_SELECTED);
    } else if (s_cleared) {
        ddj_row_update(&s_reset, "重置进度", "已清零",
                       s_sel == 1 ? DDJ_STATE_SELECTED : DDJ_STATE_NORMAL);
    } else {
        ddj_row_update(&s_reset, "重置进度", "不可恢复",
                       s_sel == 1 ? DDJ_STATE_SELECTED : DDJ_STATE_NORMAL);
    }
}

static void settings_cancel_confirm(void)
{
    s_confirm = false;
    if (s_hint) lv_label_set_text(s_hint, HINT_IDLE);
    settings_refresh();
}

lv_obj_t *ddj_settings_enter(void)
{
    s_sel = 0;
    s_confirm = false;
    s_cleared = false;
    s_hint = NULL;
    s_sound = (ddj_row_t){0};
    s_reset = (ddj_row_t){0};

    lv_obj_t *card = NULL;
    lv_obj_t *scr = ddj_page_create(&card);
    if (!card) return scr;

    ddj_topbar_create(card, "设置");
    s_hint = ddj_hint_create(card, HINT_IDLE);
    lv_obj_t *body = ddj_body_create(card);

    s_sound = ddj_row_create(body, DDJ_BODY_X, SETTINGS_ROW_Y, DDJ_BODY_W, DDJ_ROW_H,
                             DDJ_ROW_NOTE_W);
    s_reset = ddj_row_create(body, DDJ_BODY_X,
                             SETTINGS_ROW_Y + DDJ_ROW_H + SETTINGS_ROW_GAP, DDJ_BODY_W,
                             DDJ_ROW_H, DDJ_ROW_NOTE_W);

    lv_obj_t *panel = ddj_panel_create(body, DDJ_BODY_X, SETTINGS_INFO_Y, DDJ_BODY_W,
                                       SETTINGS_INFO_H, DDJ_C_PAPER_ALT, DDJ_C_LINE, 3);
    lv_obj_t *title = ddj_label_create(panel, "道德经 · 日课", &ddj_font_16, DDJ_C_INK);
    if (title) lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    // 字面量 29 字节 + 两个最宽 %d（各 11）+ 结尾符 = 52，取 56。
    char meta[56];
    snprintf(meta, sizeof(meta), "全本 %d 章 · 已收录 %d 章", ddj_chapter_total(),
             ddj_chapter_count());
    lv_obj_t *line = ddj_label_create(panel, meta, &ddj_font_16, DDJ_C_INK_SOFT);
    if (line) lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 42);

    lv_obj_t *base = ddj_label_create(panel, "基于 FoloToy AI Passport", &ddj_font_16,
                                      DDJ_C_MUTED);
    if (base) lv_obj_align(base, LV_ALIGN_TOP_MID, 0, 72);

    settings_refresh();
    return scr;
}

void ddj_settings_leave(void)
{
    s_hint = NULL;
    s_sound = (ddj_row_t){0};
    s_reset = (ddj_row_t){0};
    s_confirm = false;
}

void ddj_settings_key(ddj_key_t key)
{
    // 确认状态下，除「确定」以外的任何动作都算反悔。
    if (s_confirm && key != DDJ_KEY_OK) {
        settings_cancel_confirm();
        return;
    }

    switch (key) {
        case DDJ_KEY_UP:
            if (s_sel == 0) return;
            s_sel = 0;
            ddj_sound_play(DDJ_SOUND_MOVE);
            settings_refresh();
            break;
        case DDJ_KEY_DOWN:
            if (s_sel == 1) return;
            s_sel = 1;
            ddj_sound_play(DDJ_SOUND_MOVE);
            settings_refresh();
            break;
        case DDJ_KEY_OK:
            if (s_sel == 0) {
                const bool enabled = !ddj_sound_enabled();
                ddj_sound_set_enabled(enabled);
                (void)ddj_store_set_audio_enabled(enabled);
                // 开启时用一声进入音作为确认；关闭时本来就该是安静的。
                ddj_sound_play(DDJ_SOUND_ENTER);
                settings_refresh();
            } else if (s_confirm) {
                s_confirm = false;
                const bool ok = (ddj_store_reset() == ESP_OK);
                s_cleared = ok;
                ddj_sound_play(ok ? DDJ_SOUND_SEAL : DDJ_SOUND_BLOCK);
                if (s_hint) {
                    lv_label_set_text(s_hint, ok ? "进度已清零" : "写入失败，请重试");
                }
                settings_refresh();
            } else {
                s_confirm = true;
                s_cleared = false;
                ddj_sound_play(DDJ_SOUND_BLOCK);
                if (s_hint) lv_label_set_text(s_hint, HINT_CONFIRM);
                settings_refresh();
            }
            break;
        case DDJ_KEY_BACK:
            ddj_sound_play(DDJ_SOUND_BACK);
            ddj_app_goto_home();
            break;
        default:
            break;
    }
}
