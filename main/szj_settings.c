// main/szj_settings.c —— 设置:音效开关、重置学习进度。
//
// 重置不可逆,所以要走两步:第一次按确定只把这一行变成红色警告,再按一次才真动手,
// 期间任何别的按键都取消。孩子乱按两下就能清空进度,是设计事故。
#include "szj_settings.h"

#include <stddef.h>

#include "szj_sound.h"
#include "szj_store.h"
#include "szj_text.h"
#include "szj_ui.h"

#define SETTINGS_ROW_Y    20
#define SETTINGS_ROW_H    40
#define SETTINGS_ROW_GAP  8
#define SETTINGS_INFO_Y   130
#define SETTINGS_INFO_H   96

static szj_row_t s_sound;
static szj_row_t s_reset;
static lv_obj_t *s_hint;
static int s_sel;
static bool s_confirm;
static bool s_cleared;

static const char *const HINT_IDLE = "上/下 选择 · 确定 修改";
static const char *const HINT_CONFIRM = "再按一次确定 清空进度";

static void settings_refresh(void)
{
    szj_row_update(&s_sound, "音效", szj_sound_enabled() ? "开" : "关",
                   s_sel == 0 ? SZJ_STATE_SELECTED : SZJ_STATE_NORMAL);

    if (s_confirm) {
        szj_row_update(&s_reset, "再按一次", "", SZJ_STATE_WRONG);
    } else if (s_cleared) {
        szj_row_update(&s_reset, "重置进度", "已清零",
                       s_sel == 1 ? SZJ_STATE_SELECTED : SZJ_STATE_NORMAL);
    } else {
        szj_row_update(&s_reset, "重置进度", "不可恢复",
                       s_sel == 1 ? SZJ_STATE_SELECTED : SZJ_STATE_NORMAL);
    }
}

static void settings_cancel_confirm(void)
{
    s_confirm = false;
    if (s_hint) lv_label_set_text(s_hint, HINT_IDLE);
    settings_refresh();
}

lv_obj_t *szj_settings_enter(void)
{
    s_sel = 0;
    s_confirm = false;
    s_cleared = false;
    s_hint = NULL;
    s_sound = (szj_row_t){0};
    s_reset = (szj_row_t){0};

    lv_obj_t *card = NULL;
    lv_obj_t *scr = szj_page_create(&card);
    if (!card) return scr;

    szj_topbar_create(card, "设置");
    s_hint = szj_hint_create(card, HINT_IDLE);
    lv_obj_t *body = szj_body_create(card);

    s_sound = szj_row_create(body, SZJ_BODY_X, SETTINGS_ROW_Y, SZJ_BODY_W, SETTINGS_ROW_H);
    s_reset = szj_row_create(body, SZJ_BODY_X, SETTINGS_ROW_Y + SETTINGS_ROW_H + SETTINGS_ROW_GAP,
                             SZJ_BODY_W, SETTINGS_ROW_H);

    lv_obj_t *panel = szj_panel_create(body, SZJ_BODY_X, SETTINGS_INFO_Y, SZJ_BODY_W,
                                       SETTINGS_INFO_H, SZJ_C_PAPER_ALT, SZJ_C_LINE, 3);
    lv_obj_t *title = szj_label_create(panel, "三字经儿童学习游戏", &szj_font_16, SZJ_C_INK);
    if (title) lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t *meta = szj_label_create(panel, "404 句 · 1212 字 · 101 课", &szj_font_16,
                                      SZJ_C_INK_SOFT);
    if (meta) lv_obj_align(meta, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_t *base = szj_label_create(panel, "基于 FoloToy AI Passport", &szj_font_16,
                                      SZJ_C_MUTED);
    if (base) lv_obj_align(base, LV_ALIGN_TOP_MID, 0, 70);

    settings_refresh();
    return scr;
}

void szj_settings_leave(void)
{
    s_hint = NULL;
    s_sound = (szj_row_t){0};
    s_reset = (szj_row_t){0};
    s_confirm = false;
}

void szj_settings_key(szj_key_t key)
{
    // 确认状态下,除"确定"以外的任何动作都算反悔。
    if (s_confirm && key != SZJ_KEY_OK) {
        settings_cancel_confirm();
        return;
    }

    switch (key) {
        case SZJ_KEY_UP:
            if (s_sel == 0) return;
            s_sel = 0;
            szj_sound_play(SZJ_SOUND_MOVE);
            settings_refresh();
            break;
        case SZJ_KEY_DOWN:
            if (s_sel == 1) return;
            s_sel = 1;
            szj_sound_play(SZJ_SOUND_MOVE);
            settings_refresh();
            break;
        case SZJ_KEY_OK:
            if (s_sel == 0) {
                const bool enabled = !szj_sound_enabled();
                szj_sound_set_enabled(enabled);
                (void)szj_store_set_audio_enabled(enabled);
                // 开启时用一声进入音作为确认;关闭时本来就该是安静的。
                szj_sound_play(SZJ_SOUND_ENTER);
                settings_refresh();
            } else if (s_confirm) {
                s_confirm = false;
                const bool ok = (szj_store_reset() == ESP_OK);
                s_cleared = ok;
                szj_sound_play(ok ? SZJ_SOUND_LESSON_DONE : SZJ_SOUND_WRONG);
                if (s_hint) lv_label_set_text(s_hint, ok ? "进度已清零" : "写入失败，请重试");
                settings_refresh();
            } else {
                s_confirm = true;
                s_cleared = false;
                szj_sound_play(SZJ_SOUND_WRONG);
                if (s_hint) lv_label_set_text(s_hint, HINT_CONFIRM);
                settings_refresh();
            }
            break;
        case SZJ_KEY_BACK:
            szj_sound_play(SZJ_SOUND_BACK);
            szj_app_goto_home();
            break;
        default:
            break;
    }
}
