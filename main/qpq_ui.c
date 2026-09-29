// main/qpq_ui.c —— 见 qpq_ui.h。移植自本仓库前一个应用（道德经日课）的 ddj_ui.c，
// 版式几何与控件行为未改；配色换成侨批的泛黄信纸，并加入「对／错」两态。
#include "qpq_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"
#include "qpq_wrap.h"

// 「每行几个字」是算术，不是手感。三个预算各自乘以自己的字号，都必须放得进
// 内容区的 210px —— 放不进就说明内容区的宽度或者字号被改过，而另一处没跟上。
_Static_assert(QPQ_CHARS_HEADLINE * 32 <= QPQ_BODY_W, "32px 每行字数不再放得下");
_Static_assert(QPQ_CHARS_BODY * 24 <= QPQ_BODY_W, "24px 每行字数不再放得下");
_Static_assert(QPQ_CHARS_SMALL * 16 <= QPQ_BODY_W, "16px 每行字数不再放得下");

// 生成的题库上限与这里的折行预算必须彼此相容。这几条把「内容侧守门值」与
// 「屏上可排字数」绑在一起：改了任何一边，另一边没跟上就编译不过。
_Static_assert(QPQ_OPTION_MAX_CHARS <= QPQ_CHARS_BODY,
               "候选要在一行内放得下（含左侧甲乙丙丁标记）");
_Static_assert(QPQ_CATEGORY_MAX_CHARS <= QPQ_CHARS_SMALL,
               "分类要与进度数字同处顶栏一行");
_Static_assert(QPQ_SENTENCE_MAX_CHARS <= 3 * QPQ_CHARS_BODY,
               "句子预算与答题页给句子的 3 行不再相容");
// 折行缓冲要装得下最长文案：60 字 x 3 字节 + 换行 + NUL。换行按最坏 10 个估。
_Static_assert(QPQ_EXPLAIN_MAX_CHARS * 3 + 10 + 1 <= QPQ_WRAP_CAPACITY,
               "折行缓冲装不下最长的解析文案");

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

lv_obj_t *qpq_label_create(lv_obj_t *parent, const char *text,
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

// 折行缓冲。模块内一份就够：每次都是「折好 -> 拷进标签」一气做完，
// 调用方拿到标签之后就不再引用这块内存。
static char s_wrapped[QPQ_WRAP_CAPACITY];

void qpq_text_set(lv_obj_t *label, const char *text, int chars_per_line)
{
    if (!label) return;

    const size_t length =
        qpq_wrap_utf8(text, chars_per_line, s_wrapped, sizeof(s_wrapped));
    if (length == 0) {
        // 折行返回 0 只有两种原因：容量不够（缓冲区开小了，属编程错误），
        // 或者入参为空/非法（正常路径上就是空串）。两种都写空串 ——
        // 把上一屏的残字留在标签上，是最难查的一类 bug。
        lv_label_set_text(label, "");
        return;
    }
    lv_label_set_text(label, s_wrapped);
}

#define QPQ_PANEL_RADIUS 12

// 返回【无边框的透明容器】：边框用两层实心圆角矩形自己叠出来，而不是用
// border 属性。这样子控件的坐标从容器左上角干净起算，不必去猜「边框算不算
// 进内容区」这种内部细节。
lv_obj_t *qpq_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg, uint32_t border, int border_w)
{
    lv_obj_t *outer = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_bg_opa(outer, LV_OPA_TRANSP, 0);

    if (border_w > 0) {
        lv_obj_t *frame = filled(outer, 0, 0, w, h, border);
        lv_obj_set_style_radius(frame, QPQ_PANEL_RADIUS, 0);
    }

    const int inset = border_w > 0 ? border_w : 0;
    lv_obj_t *inner = filled(outer, inset, inset, w - 2 * inset, h - 2 * inset, bg);
    lv_obj_set_style_radius(inner, QPQ_PANEL_RADIUS - inset, 0);
    return outer;
}

lv_obj_t *qpq_page_create(lv_obj_t **out_card)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(QPQ_C_SCREEN), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, QPQ_PAGE_INSET, QPQ_PAGE_INSET);
    lv_obj_set_size(card, QPQ_PAGE_W, QPQ_PAGE_H);
    lv_obj_set_style_radius(card, QPQ_PAGE_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(QPQ_C_PAPER), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    // 顶/底栏是直角矩形，靠裁角让步进卡片的圆角里，不靠「颜色碰巧一样」兜住。
    lv_obj_set_style_clip_corner(card, true, 0);

    if (out_card) *out_card = card;
    return scr;
}

lv_obj_t *qpq_body_create(lv_obj_t *card)
{
    lv_obj_t *body = plain_obj(card, 0, QPQ_BODY_TOP, QPQ_PAGE_W, QPQ_BODY_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    return body;
}

lv_obj_t *qpq_scroll_create(lv_obj_t *body)
{
    // 容器自己可滚动（这是本应用里唯一允许滚动的对象）。判卷页的内容 ——
    // 解析 + 完整原文 + 出处 —— 本来就不可能塞进 248px，与其砍内容，不如让它滚。
    lv_obj_t *container = plain_obj(body, 0, 0, QPQ_PAGE_W, QPQ_BODY_H);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(container, LV_DIR_VER);
    lv_obj_set_style_pad_top(container, 2, 0);
    lv_obj_set_style_pad_bottom(container, 2, 0);
    return container;
}

// 电池外壳 20x11。外框自己用四条实心边画，不用 border —— 这样 shell 的坐标
// 原点就是它的左上角。
#define QPQ_BAT_W 20
#define QPQ_BAT_H 11
#define QPQ_BAT_EDGE 2

static void battery_create(lv_obj_t *bar)
{
    const int soc = bsp_battery_soc();
    const int pct = soc > 100 ? 100 : soc;

    char text[16];
    if (soc >= 0) {
        snprintf(text, sizeof(text), "%d%%", pct);
    } else {
        snprintf(text, sizeof(text), "--");
    }

    lv_obj_t *label = qpq_label_create(bar, text, &qpq_font_16, QPQ_C_HINTINK);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, -40, 0);

    lv_obj_t *shell = plain_obj(bar, 0, 0, QPQ_BAT_W, QPQ_BAT_H);
    lv_obj_set_style_bg_opa(shell, LV_OPA_TRANSP, 0);
    lv_obj_align(shell, LV_ALIGN_RIGHT_MID, -13, 0);

    filled(shell, 0, 0, QPQ_BAT_W, QPQ_BAT_EDGE, QPQ_C_HINTINK);
    filled(shell, 0, QPQ_BAT_H - QPQ_BAT_EDGE, QPQ_BAT_W, QPQ_BAT_EDGE, QPQ_C_HINTINK);
    filled(shell, 0, 0, QPQ_BAT_EDGE, QPQ_BAT_H, QPQ_C_HINTINK);
    filled(shell, QPQ_BAT_W - QPQ_BAT_EDGE, 0, QPQ_BAT_EDGE, QPQ_BAT_H, QPQ_C_HINTINK);

    // 正极触点与外壳同层，否则会被外壳裁掉。
    lv_obj_t *cap = filled(bar, 0, 0, 3, 5, QPQ_C_HINTINK);
    lv_obj_set_style_radius(cap, 1, 0);
    lv_obj_align(cap, LV_ALIGN_RIGHT_MID, -10, 0);

    if (soc > 0) {
        const int usable = QPQ_BAT_W - 2 * (QPQ_BAT_EDGE + 1);
        int width = (usable * pct) / 100;
        if (width < 1) width = 1;
        // 电量本身没有褒贬，用一档中性灰墨，不引入第三种彩色。
        lv_obj_t *fill = filled(shell, QPQ_BAT_EDGE + 1, QPQ_BAT_EDGE + 1, width,
                                QPQ_BAT_H - 2 * (QPQ_BAT_EDGE + 1), QPQ_C_MUTED);
        lv_obj_set_style_radius(fill, 2, 0);
    }
}

// 顶栏左右两侧各留多少宽度：左侧 14 起，右侧 56 封顶（进度 "20/20" 与电量都要
// 放得进这个区间）。
#define QPQ_BAR_PAD        14
#define QPQ_BAR_RIGHT_W    56
#define QPQ_BAR_LEFT_W     (QPQ_PAGE_W - QPQ_BAR_PAD - QPQ_BAR_RIGHT_W - 6)

qpq_topbar_t qpq_topbar_create(lv_obj_t *card, const char *left, const lv_font_t *left_font)
{
    qpq_topbar_t bar = {0};
    bar.bar = filled(card, 0, 0, QPQ_PAGE_W, QPQ_BAR_H, QPQ_C_INK);

    if (left && left[0]) {
        bar.left = qpq_label_create(bar.bar, left, left_font ? left_font : &qpq_font_24,
                                    QPQ_C_PAPER);
        lv_obj_set_width(bar.left, QPQ_BAR_LEFT_W);
        lv_obj_set_height(bar.left, LV_SIZE_CONTENT);
        lv_label_set_long_mode(bar.left, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(bar.left, LV_ALIGN_LEFT_MID, QPQ_BAR_PAD, 0);
    }
    return bar;
}

void qpq_topbar_set_left(qpq_topbar_t *bar, const char *text)
{
    if (!bar || !bar->left) return;
    lv_label_set_text(bar->left, text ? text : "");
}

void qpq_topbar_set_right(qpq_topbar_t *bar, const char *text)
{
    if (!bar || !bar->bar) return;
    if (bar->right) {
        lv_obj_delete(bar->right);
        bar->right = NULL;
    }
    bar->right = qpq_label_create(bar->bar, text ? text : "", &qpq_font_16, QPQ_C_HINTINK);
    lv_obj_set_width(bar->right, QPQ_BAR_RIGHT_W);
    lv_obj_set_height(bar->right, LV_SIZE_CONTENT);
    lv_label_set_long_mode(bar->right, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(bar->right, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(bar->right, LV_ALIGN_RIGHT_MID, -QPQ_BAR_PAD, 0);
}

void qpq_topbar_add_battery(qpq_topbar_t *bar)
{
    if (!bar || !bar->bar) return;
    battery_create(bar->bar);
}

lv_obj_t *qpq_hint_create(lv_obj_t *card, const char *text)
{
    lv_obj_t *bar = filled(card, 0, QPQ_PAGE_H - QPQ_HINT_H, QPQ_PAGE_W, QPQ_HINT_H,
                           QPQ_C_INK);
    lv_obj_t *label = qpq_label_create(bar, text, &qpq_font_16, QPQ_C_HINTINK);
    // 底栏只有 26px 高，一行 16px 字占 20px。宽度封死并禁用换行 ——
    // 文案变长时宁可在右边打省略号，也不能折成两行顶出底栏。
    lv_obj_set_width(label, QPQ_PAGE_W - 12);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    return label;
}

lv_obj_t *qpq_headline_create(lv_obj_t *parent, int x, int y, int w, int h,
                              uint32_t color)
{
    // 折行模式必须在设完宽度之后再设，否则 LVGL 会先按内容算一次尺寸。
    lv_obj_t *label = qpq_label_create(parent, "", &qpq_font_32, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label, 4, 0);
    return label;
}

lv_obj_t *qpq_paragraph_create(lv_obj_t *parent, int x, int y, int w, int h,
                               const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = qpq_label_create(parent, "", font, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    // 行距压到 4px：24px 字库的行高本身是 29px。判卷页虽然可滚动，但行距松一格
    // 就多滚一屏，读起来反而累。
    lv_obj_set_style_text_line_space(label, 4, 0);
    return label;
}

lv_obj_t *qpq_note_create(lv_obj_t *parent, int x, int y, int w,
                          const char *text, uint32_t color)
{
    lv_obj_t *label = qpq_label_create(parent, text, &qpq_font_16, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    return label;
}

static void row_apply(qpq_row_t *row, qpq_state_t state)
{
    if (!row || !row->box) return;

    uint32_t bg = QPQ_C_PAPER_ALT;
    uint32_t border = QPQ_C_LINE;
    uint32_t ink = QPQ_C_INK;
    uint32_t note_ink = QPQ_C_MUTED;

    switch (state) {
        case QPQ_STATE_SELECTED:
            bg = QPQ_C_CINNABAR_S;
            border = QPQ_C_CINNABAR;
            ink = QPQ_C_CINNABAR_D;
            note_ink = QPQ_C_CINNABAR_D;
            break;
        case QPQ_STATE_CORRECT:
            bg = QPQ_C_GREEN_S;
            border = QPQ_C_GREEN;
            ink = QPQ_C_GREEN;
            note_ink = QPQ_C_GREEN;
            break;
        case QPQ_STATE_WRONG:
            bg = QPQ_C_CINNABAR_S;
            border = QPQ_C_CINNABAR;
            ink = QPQ_C_CINNABAR_D;
            note_ink = QPQ_C_CINNABAR_D;
            break;
        case QPQ_STATE_NORMAL:
        default:
            break;
    }

    lv_obj_set_style_bg_color(row->box, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(row->box, lv_color_hex(border), 0);
    if (row->tag) lv_obj_set_style_text_color(row->tag, lv_color_hex(ink), 0);
    if (row->text) lv_obj_set_style_text_color(row->text, lv_color_hex(ink), 0);
    if (row->note) lv_obj_set_style_text_color(row->note, lv_color_hex(note_ink), 0);
}

qpq_row_t qpq_row_create(lv_obj_t *parent, int x, int y, int w, int h,
                         int tag_width, int note_width)
{
    qpq_row_t row = {0};
    row.box = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_radius(row.box, 8, 0);
    lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
    // 边框宽度恒为 3，状态切换只换颜色，文字不会跳。
    lv_obj_set_style_border_width(row.box, QPQ_ROW_BORDER, 0);

    // 左侧标记与右侧状态先把宽度占掉，主文字用剩下的 —— 状态永远贴着右边，
    // 不会因为主文字变长而被顶走。宽度为 0 表示这一侧没有东西。
    const int tag_w = tag_width > 0 ? tag_width : 0;
    const int note_w = note_width > 0 ? note_width : 0;
    const int inner = w - 2 * QPQ_ROW_BORDER - 2 * QPQ_ROW_PAD - tag_w - note_w;

    if (tag_w > 0) {
        // 标记用 24px 同一档，与选项正文齐平；不单独做一个小字号的印章方框，
        // 免得在 36px 的行高里塞两级字号反而更乱。
        row.tag = qpq_label_create(row.box, "", &qpq_font_24, QPQ_C_INK);
        lv_obj_set_width(row.tag, tag_w);
        lv_obj_set_height(row.tag, LV_SIZE_CONTENT);
        lv_obj_align(row.tag, LV_ALIGN_LEFT_MID, QPQ_ROW_PAD, 0);
    }

    // 文字宽度写死，不让它按内容自己长；真放不下时 LVGL 打省略号，
    // 不会溢出到边框外面。
    row.text = qpq_label_create(row.box, "", &qpq_font_24, QPQ_C_INK);
    lv_obj_set_width(row.text, inner);
    lv_obj_set_height(row.text, LV_SIZE_CONTENT);
    lv_label_set_long_mode(row.text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(row.text, LV_ALIGN_LEFT_MID, QPQ_ROW_PAD + tag_w, 0);

    if (note_w > 0) {
        row.note = qpq_label_create(row.box, "", &qpq_font_16, QPQ_C_MUTED);
        lv_obj_set_width(row.note, note_w);
        lv_obj_set_height(row.note, LV_SIZE_CONTENT);
        lv_label_set_long_mode(row.note, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(row.note, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(row.note, LV_ALIGN_RIGHT_MID, -QPQ_ROW_PAD, 0);
    }

    row_apply(&row, QPQ_STATE_NORMAL);
    return row;
}

void qpq_row_update(qpq_row_t *row, const char *tag, const char *text,
                    const char *note, qpq_state_t state)
{
    if (!row || !row->box) return;
    if (row->tag) lv_label_set_text(row->tag, tag ? tag : "");
    if (row->text) lv_label_set_text(row->text, text ? text : "");
    if (row->note) lv_label_set_text(row->note, note ? note : "");
    row_apply(row, state);
}

void qpq_row_set_state(qpq_row_t *row, qpq_state_t state)
{
    row_apply(row, state);
}
