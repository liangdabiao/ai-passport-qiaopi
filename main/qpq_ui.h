// main/qpq_ui.h —— 界面层：主题、字体与可复用控件。
//
// 配色取自网页版《侨批 · 填字问答》的 CSS 变量（泛黄信纸 + 墨 + 朱红 + 一点石绿），
// 与上一个应用（道德经日课的「素宣墨朱」）同族但有区别：这里的纸更黄、更旧，
// 并且多了「对／错」两态 —— 日课不判对错，这一版要判卷。
//
// 版式沿用同一块屏的物理事实：240x320，四角有 30px 圆角遮罩。逻辑屏底取深墨
// （与遮罩的纯黑过渡自然），真正的页面是一张四周内缩 5px、圆角 25px 的信纸卡片 ——
// 半径 25 < 遮罩半径 30，卡片本身完整落在可见区里。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

// 版式预算与生成的题库上限是同一套数字的两端，所以这里显式依赖生成物：
// qpq_ui.c 用 _Static_assert 把「预算 x 字号 <= 内容区宽」与「内容上限 <= 每行
// 预算」钉在编译期，改了任何一边另一边没跟上就编译不过。
#include "qpq_text.h"

// ---------------------------------------------------------------- 配色 -----
// 全部由网页版的十六进制值转成 RGB565（R5 G6 B5），转换过程记在注释里，
// 免得以后有人对着一个 0xF758 猜它原本是什么颜色。
#define QPQ_C_SCREEN     0x18A2   // 逻辑屏底：#1c1510 深墨
#define QPQ_C_PAPER      0xF758   // 页面底：#f4e8c1 泛黄信纸
#define QPQ_C_PAPER_ALT  0xEED6   // 次级底：#e8d8b0 信纸暗边（未选中面板）
#define QPQ_C_INK        0x28C2   // 墨：#2c1810 正文与顶/底栏
#define QPQ_C_INK_SOFT   0x59C5   // 次级文字：#5c3a28
#define QPQ_C_MUTED      0x8B8A   // 提示文字：#8b7355
#define QPQ_C_LINE       0xDE54   // 行边框：#dcc9a3
#define QPQ_C_CINNABAR   0xC1C4   // 朱红：#c23b22 强调与当前选择
#define QPQ_C_CINNABAR_D 0x9963   // 朱红深：#9a2e1a 选中行里的文字
#define QPQ_C_CINNABAR_S 0xF71A   // 朱红淡：#f7e0d4 选中行的底
#define QPQ_C_HINTINK    0xBD10   // 底栏提示文字：#b8a080（压在墨色栏上）
#define QPQ_C_GOLD       0xBCA7   // 石金：#b8963e 印章与评级
#define QPQ_C_GREEN      0x2BE8   // 对：#2d7d46
#define QPQ_C_GREEN_S    0xDF7B   // 对的底：#dcecd8

// --------------------------------------------------------------- 版式 ------
#define QPQ_SCREEN_W     240
#define QPQ_SCREEN_H     320
#define QPQ_PAGE_INSET   5
#define QPQ_PAGE_W       (QPQ_SCREEN_W - 2 * QPQ_PAGE_INSET)          // 230
#define QPQ_PAGE_H       (QPQ_SCREEN_H - 2 * QPQ_PAGE_INSET)          // 310
#define QPQ_PAGE_RADIUS  25
#define QPQ_BAR_H        36
#define QPQ_HINT_H       26
#define QPQ_BODY_TOP     QPQ_BAR_H                                    // 36
#define QPQ_BODY_BOTTOM  (QPQ_PAGE_H - QPQ_HINT_H)                    // 284
#define QPQ_BODY_H       (QPQ_BODY_BOTTOM - QPQ_BODY_TOP)             // 248
#define QPQ_BODY_X       10
#define QPQ_BODY_W       (QPQ_PAGE_W - 2 * QPQ_BODY_X)                // 210

// ------------------------------------------------------- 每行字数预算 -----
// 「一屏放得下」不是感觉，是一道除法：内容区宽 210px，同一字号下汉字等宽
// （Noto Sans CJK 的全角字形，前进宽度就等于字号），所以每行几个字 = 除法取整。
// 这三个数字必须与 tools/qiaopi/content.py 顶部的上限、以及
// tests/test_qpq_wrap.c 的断言一致 —— 三处一旦不一致，就会出现「生成阶段过了、
// 屏上却溢了」。qpq_ui.c 用 _Static_assert 把它们钉在编译期。
#define QPQ_CHARS_HEADLINE (QPQ_BODY_W / 32)   // 6
#define QPQ_CHARS_BODY     (QPQ_BODY_W / 24)   // 8
#define QPQ_CHARS_SMALL    (QPQ_BODY_W / 16)   // 13

// --------------------------------------------------------------- 字体 ------
// 由 tools/qiaopi/gen_font.py 生成到 assets/fonts/，覆盖题库、解析、出处、结算
// 文案与全部界面用字。没有 fallback：漏字在生成阶段就报错，不会到屏上变成空白框。
LV_FONT_DECLARE(qpq_font_16);
LV_FONT_DECLARE(qpq_font_24);
LV_FONT_DECLARE(qpq_font_32);

// ------------------------------------------------------------- 控件状态 ----
// 比上一个应用多两态：这一版要判卷，正确项与选错项必须能一眼分开。
typedef enum {
    QPQ_STATE_NORMAL = 0,
    QPQ_STATE_SELECTED,   // 光标停在这一项
    QPQ_STATE_CORRECT,    // 判卷后：正确答案
    QPQ_STATE_WRONG,      // 判卷后：玩家选错的那一项
} qpq_state_t;

// 列表行：一个圆角面板 + 可选的左侧标记（甲乙丙丁）+ 主文字 + 右侧状态小字。
// 标记与状态都可以为空 —— 与其编一个「无」出来，不如留白。
typedef struct {
    lv_obj_t *box;
    lv_obj_t *tag;
    lv_obj_t *text;
    lv_obj_t *note;
} qpq_row_t;

// ------------------------------------------------------------- 页面骨架 ----
// 建逻辑屏（深墨底）与页面卡（信纸）。返回屏幕根对象供 lv_scr_load /
// lv_obj_delete 使用；*out_card 回填页面卡，后续控件挂在它上面。
lv_obj_t *qpq_page_create(lv_obj_t **out_card);

// 内容区容器：夹在顶栏与底栏之间，子控件坐标从它的左上角起算。
lv_obj_t *qpq_body_create(lv_obj_t *card);

// 可滚动正文容器（判卷页用）。全应用只有需要滚动的页面才建它。
lv_obj_t *qpq_scroll_create(lv_obj_t *body);

// 顶栏（墨底）：左侧标题 + 右侧任意内容。
typedef struct {
    lv_obj_t *bar;
    lv_obj_t *left;
    lv_obj_t *right;
} qpq_topbar_t;

qpq_topbar_t qpq_topbar_create(lv_obj_t *card, const char *left, const lv_font_t *left_font);
void qpq_topbar_set_left(qpq_topbar_t *bar, const char *text);
// 右侧文字（进度 "3/20" 这种每次刷新都要变的）。会替换掉先前的右侧内容。
void qpq_topbar_set_right(qpq_topbar_t *bar, const char *text);
// 在右侧画电池指示。读不到电量时显示 "--"，不编一个数。
void qpq_topbar_add_battery(qpq_topbar_t *bar);

// 底栏提示条（墨底）。返回其中的文字标签，便于翻页时只改文字。
lv_obj_t *qpq_hint_create(lv_obj_t *card, const char *text);

// ------------------------------------------------------------- 基础控件 ----
lv_obj_t *qpq_label_create(lv_obj_t *parent, const char *text,
                           const lv_font_t *font, uint32_t color);

// 圆角面板。border_w 为 0 时不画边框。
lv_obj_t *qpq_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg, uint32_t border, int border_w);

// 居中大字（标题页的主标题）。
lv_obj_t *qpq_headline_create(lv_obj_t *parent, int x, int y, int w, int h,
                              uint32_t color);

// 一段会折行的正文，左对齐、顶端起排。
lv_obj_t *qpq_paragraph_create(lv_obj_t *parent, int x, int y, int w, int h,
                               const lv_font_t *font, uint32_t color);

// 把一段中文折行后写进标签。折行缓冲是模块内的静态区，一次调用写完就拷进标签，
// 所以调用方不用自己开缓冲区，也不用管它的生命周期。
void qpq_text_set(lv_obj_t *label, const char *text, int chars_per_line);

// 小字标签。
lv_obj_t *qpq_note_create(lv_obj_t *parent, int x, int y, int w,
                          const char *text, uint32_t color);

// ---------------------------------------------------------------- 行 ------
#define QPQ_ROW_BORDER 3
#define QPQ_ROW_PAD    8
// 左侧标记（甲/乙/丙/丁）占的宽度。24px 字下一个汉字宽 24px，26 留出余量。
#define QPQ_ROW_TAG_W  26
// 行高 34：24px 字库的行高是 29px，加 3+3 边框是 35 —— 所以 34 装不下一个 24px
// 字，这里取 36。答题页要放下四行（4x36 + 3x4 间隔 = 156px），加上句子 3 行
// （87px）恰好 243px，落在 248px 的内容区里。
#define QPQ_ROW_H      36
#define QPQ_ROW_GAP    4

// tag_width 为 0 表示这一行没有左侧标记；note_width 为 0 表示没有右侧状态。
qpq_row_t qpq_row_create(lv_obj_t *parent, int x, int y, int w, int h,
                         int tag_width, int note_width);
void qpq_row_update(qpq_row_t *row, const char *tag, const char *text,
                    const char *note, qpq_state_t state);

// 只改状态，不动文字。
//
// 为什么不复用 qpq_row_update：那需要把当前文字再传一遍，而「取当前文字」只有
// lv_label_get_text 一条路 —— 它返回的是标签内部的缓冲区，而 lv_label_set_text
// 会先 free 旧文本再复制，于是「把那个指针喂回同一个标签」就是 use-after-free。
// 与其在调用处小心规避，不如在这里开一个只碰状态的入口。
void qpq_row_set_state(qpq_row_t *row, qpq_state_t state);
