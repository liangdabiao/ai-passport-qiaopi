// main/ddj_ui.c —— 见 ddj_ui.h。
#include "ddj_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"

// 所有控件都关掉滚动、清掉默认内边距，位置一律用绝对坐标 —— 尺寸在编译期
// 就算死，不依赖主题默认值，也就不会「换个主题就错位」。
static lv_obj_t *plain_obj(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *filled(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

lv_obj_t *ddj_label_create(lv_obj_t *parent, const char *text,
                           const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    return label;
}

#define DDJ_PANEL_RADIUS 12

// 返回【无边框的透明容器】：边框用两层实心圆角矩形自己叠出来，而不是用
// border 属性。这样子控件的坐标从容器左上角干净起算，不必去猜「边框算不算
// 进内容区」这种内部细节。
lv_obj_t *ddj_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg, uint32_t border, int border_w)
{
    lv_obj_t *outer = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);

    if (border_w > 0) {
        lv_obj_t *frame = filled(outer, 0, 0, w, h, border);
        lv_obj_set_style_radius(frame, DDJ_PANEL_RADIUS, 0);
    }

    const int inset = border_w > 0 ? border_w : 0;
    lv_obj_t *inner = filled(outer, inset, inset, w - 2 * inset, h - 2 * inset, bg);
    lv_obj_set_style_radius(inner, DDJ_PANEL_RADIUS - inset, 0);
    return outer;
}

lv_obj_t *ddj_page_create(lv_obj_t **out_card)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(DDJ_C_SCREEN), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, DDJ_PAGE_INSET, DDJ_PAGE_INSET);
    lv_obj_set_size(card, DDJ_PAGE_W, DDJ_PAGE_H);
    lv_obj_set_style_radius(card, DDJ_PAGE_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(DDJ_C_PAPER), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    // 顶/底栏是直角矩形，靠裁角让步进卡片的圆角里，不靠「颜色碰巧一样」兜住。
    lv_obj_set_style_clip_corner(card, true, 0);

    if (out_card) *out_card = card;
    return scr;
}

lv_obj_t *ddj_body_create(lv_obj_t *card)
{
    lv_obj_t *body = plain_obj(card, 0, DDJ_BODY_TOP, DDJ_PAGE_W, DDJ_BODY_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    return body;
}

// 电池外壳 20x11。外框自己用四条实心边画，不用 border —— 这样 shell 的坐标
// 原点就是它的左上角。
#define DDJ_BAT_W 20
#define DDJ_BAT_H 11
#define DDJ_BAT_EDGE 2

// 右上角电量。读不到（--）就只画空壳，不编一个数字出来。
static void battery_create(lv_obj_t *bar)
{
    const int soc = bsp_battery_soc();
    const int pct = soc > 100 ? 100 : soc;

    char text[8];
    if (soc >= 0) {
        snprintf(text, sizeof(text), "%d%%", pct);
    } else {
        snprintf(text, sizeof(text), "--");
    }

    lv_obj_t *label = ddj_label_create(bar, text, &ddj_font_16, DDJ_C_HINTINK);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, -40, 0);

    lv_obj_t *shell = plain_obj(bar, 0, 0, DDJ_BAT_W, DDJ_BAT_H);
    lv_obj_set_style_bg_opa(shell, LV_OPA_TRANSP, 0);
    lv_obj_align(shell, LV_ALIGN_RIGHT_MID, -13, 0);

    filled(shell, 0, 0, DDJ_BAT_W, DDJ_BAT_EDGE, DDJ_C_HINTINK);
    filled(shell, 0, DDJ_BAT_H - DDJ_BAT_EDGE, DDJ_BAT_W, DDJ_BAT_EDGE, DDJ_C_HINTINK);
    filled(shell, 0, 0, DDJ_BAT_EDGE, DDJ_BAT_H, DDJ_C_HINTINK);
    filled(shell, DDJ_BAT_W - DDJ_BAT_EDGE, 0, DDJ_BAT_EDGE, DDJ_BAT_H, DDJ_C_HINTINK);

    // 正极触点与外壳同层，否则会被外壳裁掉。
    lv_obj_t *cap = filled(bar, 0, 0, 3, 5, DDJ_C_HINTINK);
    lv_obj_set_style_radius(cap, 1, 0);
    lv_obj_align(cap, LV_ALIGN_RIGHT_MID, -10, 0);

    if (soc > 0) {
        const int usable = DDJ_BAT_W - 2 * (DDJ_BAT_EDGE + 1);
        int width = (usable * pct) / 100;
        if (width < 1) width = 1;
        // 电量本身没有褒贬，用一档中性灰墨，不引入第三种彩色。
        lv_obj_t *fill = filled(shell, DDJ_BAT_EDGE + 1, DDJ_BAT_EDGE + 1, width,
                                DDJ_BAT_H - 2 * (DDJ_BAT_EDGE + 1), DDJ_C_MUTED);
        lv_obj_set_style_radius(fill, 2, 0);
    }
}

void ddj_topbar_create(lv_obj_t *card, const char *title)
{
    lv_obj_t *bar = filled(card, 0, 0, DDJ_PAGE_W, DDJ_BAR_H, DDJ_C_INK);
    if (title && title[0]) {
        lv_obj_t *label = ddj_label_create(bar, title, &ddj_font_24, DDJ_C_PAPER);
        // 左边距 14，右边给电量留 14+20+3，标题宽度封在剩下的区间里：
        // 24px 字号下大约放得下 7 个汉字，再多就打省略号。
        lv_obj_set_width(label, DDJ_PAGE_W - 14 - (14 + 20 + 3) - 6);
        lv_obj_set_height(label, LV_SIZE_CONTENT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 14, 0);
    }
    battery_create(bar);
}

lv_obj_t *ddj_hint_create(lv_obj_t *card, const char *text)
{
    lv_obj_t *bar = filled(card, 0, DDJ_PAGE_H - DDJ_HINT_H, DDJ_PAGE_W, DDJ_HINT_H,
                           DDJ_C_INK);
    lv_obj_t *label = ddj_label_create(bar, text, &ddj_font_16, DDJ_C_HINTINK);
    // 底栏只有 26px 高，一行 16px 字占 20px。宽度封死并禁用换行 ——
    // 文案变长时宁可在右边打省略号，也不能折成两行顶出底栏。
    lv_obj_set_width(label, DDJ_PAGE_W - 12);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    return label;
}

lv_obj_t *ddj_passage_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    // 一屏一句的经文：32px 大字，折行，区域正中。
    // 折行必须在设宽度之后设，否则 LVGL 会先按内容算一次尺寸。
    lv_obj_t *label = ddj_label_create(parent, "", &ddj_font_32, DDJ_C_INK);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label, 6, 0);
    return label;
}

lv_obj_t *ddj_paragraph_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *label = ddj_label_create(parent, "", &ddj_font_24, DDJ_C_INK);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_line_space(label, 8, 0);
    return label;
}

lv_obj_t *ddj_note_create(lv_obj_t *parent, int x, int y, int w,
                          const char *text, uint32_t color)
{
    lv_obj_t *label = ddj_label_create(parent, text, &ddj_font_16, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    return label;
}

static void row_apply(ddj_row_t *row, ddj_state_t state)
{
    if (!row || !row->box) return;

    uint32_t bg = DDJ_C_PAPER_ALT;
    uint32_t border = DDJ_C_LINE;
    uint32_t ink = DDJ_C_INK;

    if (state == DDJ_STATE_SELECTED) {
        bg = DDJ_C_CINNABAR_S;
        border = DDJ_C_CINNABAR;
        ink = DDJ_C_CINNABAR_D;
    }

    lv_obj_set_style_bg_color(row->box, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(row->box, lv_color_hex(border), 0);
    if (row->text) lv_obj_set_style_text_color(row->text, lv_color_hex(ink), 0);
}

ddj_row_t ddj_row_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    ddj_row_t row = {0};
    row.box = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_radius(row.box, 10, 0);
    lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
    // 边框宽度恒为 3，状态切换只换颜色，文字不会跳。
    lv_obj_set_style_border_width(row.box, DDJ_ROW_BORDER, 0);

    // 文字宽度写死，不让它按内容自己长；真放不下时 LVGL 打省略号，
    // 不会溢出到边框外面。
    row.text = ddj_label_create(row.box, "", &ddj_font_24, DDJ_C_INK);
    lv_obj_set_width(row.text, w - 2 * DDJ_ROW_BORDER - 2 * DDJ_ROW_PAD);
    lv_obj_set_height(row.text, LV_SIZE_CONTENT);
    lv_label_set_long_mode(row.text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(row.text, LV_ALIGN_LEFT_MID, DDJ_ROW_PAD, 0);

    row_apply(&row, DDJ_STATE_NORMAL);
    return row;
}

void ddj_row_update(ddj_row_t *row, const char *text, ddj_state_t state)
{
    if (!row || !row->box) return;
    if (row->text) lv_label_set_text(row->text, text ? text : "");
    row_apply(row, state);
}
