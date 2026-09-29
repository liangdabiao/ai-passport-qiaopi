// main/szj_ui.c —— 见 szj_ui.h。
#include "szj_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"

// 所有控件都关掉滚动、清掉默认内边距,位置一律用绝对坐标 —— 尺寸在编译期就算死,
// 不依赖主题的默认值,也就不存在"换个主题就错位"的问题。
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

lv_obj_t *szj_label_create(lv_obj_t *parent, const char *text,
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

#define SZJ_PANEL_RADIUS 12

// 返回的是【无边框的透明容器】:边框用两层实心圆角矩形自己叠出来,而不是用
// border 属性。这样容器里的子控件坐标就从容器左上角干净起算,不必去猜"边框
// 算不算进内容区"这种 LVGL 内部细节。
lv_obj_t *szj_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg, uint32_t border, int border_w)
{
    lv_obj_t *outer = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);

    if (border_w > 0) {
        lv_obj_t *frame = filled(outer, 0, 0, w, h, border);
        lv_obj_set_style_radius(frame, SZJ_PANEL_RADIUS, 0);
    }

    const int inset = border_w > 0 ? border_w : 0;
    lv_obj_t *inner = filled(outer, inset, inset, w - 2 * inset, h - 2 * inset, bg);
    lv_obj_set_style_radius(inner, SZJ_PANEL_RADIUS - inset, 0);
    return outer;
}

lv_obj_t *szj_page_create(lv_obj_t **out_card)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(SZJ_C_INK), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, SZJ_PAGE_INSET, SZJ_PAGE_INSET);
    lv_obj_set_size(card, SZJ_PAGE_W, SZJ_PAGE_H);
    lv_obj_set_style_radius(card, SZJ_PAGE_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SZJ_C_PAPER), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    // 顶栏/底栏是直角矩形,靠裁角让步进卡片的圆角里,不靠"颜色碰巧一样"兜住。
    lv_obj_set_style_clip_corner(card, true, 0);

    if (out_card) *out_card = card;
    return scr;
}

lv_obj_t *szj_body_create(lv_obj_t *card)
{
    lv_obj_t *body = plain_obj(card, 0, SZJ_BODY_TOP, SZJ_PAGE_W, SZJ_BODY_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    return body;
}

// 电池外壳 20x11。外框自己用四条实心边画,不用 border —— 这样 shell 的坐标原点
// 就是它的左上角,内部填充的 (x, y) 不用去猜"子是相对边框内还是边框外"。
#define SZJ_BAT_W 20
#define SZJ_BAT_H 11
#define SZJ_BAT_EDGE 2

// 右上角电量。读不到(--)就只画空壳,不编一个数字出来。
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

    lv_obj_t *label = szj_label_create(bar, text, &szj_font_16, SZJ_C_HINT);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, -40, 0);

    lv_obj_t *shell = plain_obj(bar, 0, 0, SZJ_BAT_W, SZJ_BAT_H);
    lv_obj_set_style_bg_opa(shell, LV_OPA_TRANSP, 0);
    lv_obj_align(shell, LV_ALIGN_RIGHT_MID, -13, 0);

    filled(shell, 0, 0, SZJ_BAT_W, SZJ_BAT_EDGE, SZJ_C_HINT);
    filled(shell, 0, SZJ_BAT_H - SZJ_BAT_EDGE, SZJ_BAT_W, SZJ_BAT_EDGE, SZJ_C_HINT);
    filled(shell, 0, 0, SZJ_BAT_EDGE, SZJ_BAT_H, SZJ_C_HINT);
    filled(shell, SZJ_BAT_W - SZJ_BAT_EDGE, 0, SZJ_BAT_EDGE, SZJ_BAT_H, SZJ_C_HINT);

    // 正极触点与外壳同层,否则会被外壳裁掉。
    lv_obj_t *cap = filled(bar, 0, 0, 3, 5, SZJ_C_HINT);
    lv_obj_set_style_radius(cap, 1, 0);
    lv_obj_align(cap, LV_ALIGN_RIGHT_MID, -10, 0);

    if (soc > 0) {
        const int usable = SZJ_BAT_W - 2 * (SZJ_BAT_EDGE + 1);
        int width = (usable * pct) / 100;
        if (width < 1) width = 1;
        const uint32_t color = pct >= 50 ? SZJ_C_BAMBOO
                             : (pct >= 20 ? SZJ_C_GOLD : SZJ_C_RED);
        lv_obj_t *fill = filled(shell, SZJ_BAT_EDGE + 1, SZJ_BAT_EDGE + 1, width,
                                SZJ_BAT_H - 2 * (SZJ_BAT_EDGE + 1), color);
        lv_obj_set_style_radius(fill, 2, 0);
    }
}

void szj_topbar_create(lv_obj_t *card, const char *title)
{
    lv_obj_t *bar = filled(card, 0, 0, SZJ_PAGE_W, SZJ_BAR_H, SZJ_C_INK);
    if (title && title[0]) {
        lv_obj_t *label = szj_label_create(bar, title, &szj_font_24, SZJ_C_PAPER);
        // 左边距 14,右边给电量留出 14+20+3 的位置,标题宽度就封在剩下的区间里,
        // 长标题打省略号,永远不会盖到电池上。
        lv_obj_set_width(label, SZJ_PAGE_W - 14 - (14 + 20 + 3) - 6);
        lv_obj_set_height(label, LV_SIZE_CONTENT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 14, 0);
    }
    battery_create(bar);
}

lv_obj_t *szj_hint_create(lv_obj_t *card, const char *text)
{
    lv_obj_t *bar = filled(card, 0, SZJ_PAGE_H - SZJ_HINT_H, SZJ_PAGE_W, SZJ_HINT_H,
                           SZJ_C_INK);
    lv_obj_t *label = szj_label_create(bar, text, &szj_font_16, SZJ_C_HINT);
    // 底栏只有 26px 高,一行 16px 字就占 20px。宽度封死并禁用换行 ——
    // 文案变长时宁可在右边打省略号,也不能折成两行顶出底栏。
    lv_obj_set_width(label, SZJ_PAGE_W - 12);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    return label;
}

lv_obj_t *szj_scroll_emblem_create(lv_obj_t *parent, int x, int y)
{
    // 竹简:五根简、两道编绳。尺寸固定 48x46,方便首页排版先算好位置。
    lv_obj_t *emblem = plain_obj(parent, x, y, 48, 46);
    lv_obj_set_style_bg_opa(emblem, LV_OPA_TRANSP, 0);

    static const uint32_t SLAT[5] = {0xC98F3C, 0xD8A24E, 0xC98F3C, 0xD8A24E, 0xC98F3C};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *slat = filled(emblem, i * 10, 0, 8, 46, SLAT[i]);
        lv_obj_set_style_radius(slat, 3, 0);
        lv_obj_set_style_border_width(slat, 2, 0);
        lv_obj_set_style_border_color(slat, lv_color_hex(SZJ_C_INK), 0);
    }
    filled(emblem, 0, 9, 48, 3, SZJ_C_INK);
    filled(emblem, 0, 33, 48, 3, SZJ_C_INK);
    return emblem;
}

// 田字格用虚线,才像描红本里的格子;实线会看成一堆方框。
// 外框同样自己画四条边,让子控件坐标从格子左上角起算。
lv_obj_t *szj_grid_create(lv_obj_t *parent, int x, int y, int size, uint32_t color)
{
    lv_obj_t *grid = plain_obj(parent, x, y, size, size);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);

    const int edge = 2;
    filled(grid, 0, 0, size, edge, color);
    filled(grid, 0, size - edge, size, edge, color);
    filled(grid, 0, 0, edge, size, color);
    filled(grid, size - edge, 0, edge, size, color);

    const int dash = 8;
    const int step = dash + 8;
    for (int offset = 4; offset + dash <= size - 4; offset += step) {
        filled(grid, size / 2 - 1, offset, 2, dash, color);
        filled(grid, offset, size / 2 - 1, dash, 2, color);
    }
    return grid;
}

lv_obj_t *szj_dot_create(lv_obj_t *parent, int x, int y, int size)
{
    lv_obj_t *dot = plain_obj(parent, x, y, size, size);
    lv_obj_set_style_radius(dot, 4, 0);
    lv_obj_set_style_border_width(dot, 2, 0);
    szj_dot_set(dot, SZJ_DOT_PENDING);
    return dot;
}

void szj_dot_set(lv_obj_t *dot, szj_dot_state_t state)
{
    if (!dot) return;
    uint32_t bg = SZJ_C_PAPER_ALT;
    uint32_t border = SZJ_C_LINE;
    switch (state) {
        case SZJ_DOT_CURRENT:
            bg = SZJ_C_PAPER_ALT;
            border = SZJ_C_RED;
            break;
        case SZJ_DOT_CORRECT:
            bg = SZJ_C_BAMBOO;
            border = SZJ_C_BAMBOO_INK;
            break;
        case SZJ_DOT_WRONG:
            bg = SZJ_C_RED;
            border = SZJ_C_RED_DARK;
            break;
        default:
            break;
    }
    lv_obj_set_style_bg_color(dot, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dot, lv_color_hex(border), 0);
}

// 行/选项共用一套底色与边框;边框宽度恒为 3,状态切换只换颜色,文字不会跳。
#define ROW_BORDER     3
#define ROW_PAD        8
// 右侧状态小字的位置上限。已按真实字体量过最长的一档:
//「第 101 课」「第 404 句」都是 16px 下 65.8px,留 72 够用。
// 主文字可用宽度 = 行宽 - 边框 - 左右内边距 - 这 72,再长就打省略号。
// 24px 下「开始学习」96px、「重置进度」96px,都在 116 以内。
#define ROW_STATUS_W   72
static void row_apply(szj_row_t *row, szj_state_t state)
{
    if (!row || !row->box) return;

    uint32_t bg = SZJ_C_PAPER_ALT;
    uint32_t border = SZJ_C_LINE;
    uint32_t ink = SZJ_C_INK;
    uint32_t sub = SZJ_C_MUTED;

    switch (state) {
        case SZJ_STATE_SELECTED:
            bg = SZJ_C_GOLD_SOFT;
            border = SZJ_C_RED;
            ink = SZJ_C_RED_DARK;
            sub = SZJ_C_RED_DARK;
            break;
        case SZJ_STATE_CORRECT:
            bg = SZJ_C_BAMBOO_SOFT;
            border = SZJ_C_BAMBOO;
            ink = SZJ_C_BAMBOO_INK;
            sub = SZJ_C_BAMBOO_INK;
            break;
        case SZJ_STATE_WRONG:
            bg = SZJ_C_RED_SOFT;
            border = SZJ_C_RED;
            ink = SZJ_C_RED_DARK;
            sub = SZJ_C_RED_DARK;
            break;
        default:
            break;
    }

    lv_obj_set_style_bg_color(row->box, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(row->box, lv_color_hex(border), 0);
    if (row->text) lv_obj_set_style_text_color(row->text, lv_color_hex(ink), 0);
    if (row->status) lv_obj_set_style_text_color(row->status, lv_color_hex(sub), 0);
}

static szj_row_t row_box_build(lv_obj_t *parent, int x, int y, int w, int h)
{
    szj_row_t row = {0};
    row.box = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_radius(row.box, 10, 0);
    lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row.box, ROW_BORDER, 0);
    return row;
}

szj_row_t szj_row_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    szj_row_t row = row_box_build(parent, x, y, w, h);

    // 主文字宽度写死,不让它按内容自己长 —— 否则长文案会压到右侧状态字上面。
    // 固定宽度 + DOTS 长文本模式:真放不下时 LVGL 会打省略号,不会互相盖。
    const int text_w = w - 2 * ROW_BORDER - 2 * ROW_PAD - ROW_STATUS_W;
    row.text = szj_label_create(row.box, "", &szj_font_24, SZJ_C_INK);
    lv_obj_set_width(row.text, text_w);
    lv_obj_set_height(row.text, LV_SIZE_CONTENT);
    lv_label_set_long_mode(row.text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(row.text, LV_ALIGN_LEFT_MID, ROW_PAD, 0);

    row.status = szj_label_create(row.box, "", &szj_font_16, SZJ_C_MUTED);
    lv_obj_set_width(row.status, ROW_STATUS_W);
    lv_obj_set_height(row.status, LV_SIZE_CONTENT);
    lv_label_set_long_mode(row.status, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(row.status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(row.status, LV_ALIGN_RIGHT_MID, -ROW_PAD, 0);
    row_apply(&row, SZJ_STATE_NORMAL);
    return row;
}

void szj_row_update(szj_row_t *row, const char *text, const char *status,
                    szj_state_t state)
{
    if (!row || !row->box) return;
    if (row->text) lv_label_set_text(row->text, text ? text : "");
    if (row->status) lv_label_set_text(row->status, status ? status : "");
    row_apply(row, state);
}

szj_row_t szj_option_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    szj_row_t row = row_box_build(parent, x, y, w, h);
    // 选项没有右侧状态字,宽度可以放宽;同样固定宽度 + 省略号兜底。
    row.text = szj_label_create(row.box, "", &szj_font_32, SZJ_C_INK);
    lv_obj_set_width(row.text, w - 2 * ROW_BORDER - 2 * ROW_PAD);
    lv_obj_set_height(row.text, LV_SIZE_CONTENT);
    lv_label_set_long_mode(row.text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(row.text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(row.text);
    row_apply(&row, SZJ_STATE_NORMAL);
    return row;
}

void szj_option_update(szj_row_t *row, const char *text, szj_state_t state)
{
    if (!row || !row->box) return;
    if (row->text) lv_label_set_text(row->text, text ? text : "");
    row_apply(row, state);
}

void szj_stars_text(int stars, int cap, char *out, size_t capacity)
{
    if (!out || capacity == 0) return;
    out[0] = '\0';
    if (cap <= 0) return;
    if (stars > cap) stars = cap;

    size_t written = 0;
    for (int i = 0; i < cap; i++) {
        const char *glyph = (i < stars) ? "★" : "☆";
        const size_t length = strlen(glyph);
        if (written + length + 1 > capacity) break;
        memcpy(out + written, glyph, length);
        written += length;
        out[written] = '\0';
    }
}
