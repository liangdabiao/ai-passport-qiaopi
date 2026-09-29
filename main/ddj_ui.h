// main/ddj_ui.h —— 本应用自己的界面层：主题、字体与可复用控件。
//
// 与基线 demo 的 ui_pixel.* 外壳无关，也不复用三字经的界面：成人阅读的取舍
// 和儿童游戏相反 —— 没有星星、没有对错色、没有装饰性图标，只有纸、墨与一处
// 朱砂。
//
// 版式沿用同一块屏的物理事实：240x320，四角有 30px 圆角遮罩。逻辑屏底色取
// 淡墨（与遮罩的纯黑过渡自然），真正的页面是一张四周内缩 5px、圆角 25px 的
// 「素宣」卡片 —— 半径 25 < 遮罩半径 30，卡片本身完整落在可见区里。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

// ---------------------------------------------------------------- 配色 -----
// 素宣墨朱。三字经用过藤黄与竹青（儿童、有对错）；这里只用纸、墨、朱砂，
// 加上一档灰墨做次级文字。强调色只有一处用途：当前选择。
#define DDJ_C_SCREEN     0x1F1C18   // 逻辑屏底：淡墨
#define DDJ_C_PAPER      0xF7F4EC   // 页面底：素宣
#define DDJ_C_PAPER_ALT  0xEDE8DC   // 次级底：未选中/未激活的面
#define DDJ_C_INK        0x2B2620   // 墨：正文与顶/底栏
#define DDJ_C_INK_SOFT   0x5A544B   // 次级文字
#define DDJ_C_MUTED      0x8E8778   // 提示文字
#define DDJ_C_LINE       0xD8D2C4   // 行边框、分隔线
#define DDJ_C_CINNABAR   0xA8322A   // 朱砂：强调与当前选择
#define DDJ_C_CINNABAR_D 0x7E241E   // 朱砂深：选中行里的文字
#define DDJ_C_CINNABAR_S 0xF5E3DE   // 朱砂淡：选中行的底
#define DDJ_C_HINTINK    0xA79C8A   // 底栏提示文字（压在墨色栏上）

// --------------------------------------------------------------- 版式 ------
#define DDJ_SCREEN_W     240
#define DDJ_SCREEN_H     320
#define DDJ_PAGE_INSET   5
#define DDJ_PAGE_W       (DDJ_SCREEN_W - 2 * DDJ_PAGE_INSET)          // 230
#define DDJ_PAGE_H       (DDJ_SCREEN_H - 2 * DDJ_PAGE_INSET)          // 310
#define DDJ_PAGE_RADIUS  25
#define DDJ_BAR_H        36
#define DDJ_HINT_H       26
#define DDJ_BODY_TOP     DDJ_BAR_H                                    // 36
#define DDJ_BODY_BOTTOM  (DDJ_PAGE_H - DDJ_HINT_H)                    // 284
#define DDJ_BODY_H       (DDJ_BODY_BOTTOM - DDJ_BODY_TOP)             // 248
#define DDJ_BODY_X       10
#define DDJ_BODY_W       (DDJ_PAGE_W - 2 * DDJ_BODY_X)                // 210

// --------------------------------------------------------------- 字体 ------
// 由 tools/daodejing/gen_font.py 生成到 assets/fonts/，覆盖经文、点拨、参究
// 与全部界面文案用字。没有 fallback：漏字在生成阶段就报错，不会到屏上变成
// 空白框。
LV_FONT_DECLARE(ddj_font_16);
LV_FONT_DECLARE(ddj_font_24);
LV_FONT_DECLARE(ddj_font_32);

// ------------------------------------------------------------- 控件状态 ----
// 只有两态。这个应用不判对错，所以不存在「正确/错误」的配色语义。
typedef enum {
    DDJ_STATE_NORMAL = 0,
    DDJ_STATE_SELECTED,
} ddj_state_t;

// 列表行：一个圆角面板 + 左侧主文字 + 右侧状态小字。
// 状态可以是空串 —— 与其编一个「无」出来，不如留白。
typedef struct {
    lv_obj_t *box;
    lv_obj_t *text;
    lv_obj_t *note;
} ddj_row_t;

// ------------------------------------------------------------- 页面骨架 ----
// 建逻辑屏（淡墨底）与页面卡（素宣）。返回屏幕根对象供 lv_screen_load /
// lv_obj_delete 使用；*out_card 回填页面卡，后续控件挂在它上面。
lv_obj_t *ddj_page_create(lv_obj_t **out_card);

// 内容区容器：夹在顶栏与底栏之间，子控件坐标从它的左上角起算。
lv_obj_t *ddj_body_create(lv_obj_t *card);

// 顶栏（墨底）：左侧标题，右侧电量。读不到电量时显示 "--"，不编一个数。
// 标题宽度按「左边距 14 + 右侧电量区 37」封死，长标题打省略号而不是盖住电量。
void ddj_topbar_create(lv_obj_t *card, const char *title);

// 底栏提示条（墨底）。返回其中的文字标签，便于翻页时只改文字。
lv_obj_t *ddj_hint_create(lv_obj_t *card, const char *text);

// ------------------------------------------------------------- 基础控件 ----
lv_obj_t *ddj_label_create(lv_obj_t *parent, const char *text,
                           const lv_font_t *font, uint32_t color);

// 圆角面板。border_w 为 0 时不画边框。
lv_obj_t *ddj_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg, uint32_t border, int border_w);

// 一屏一句的大字：给定区域里居中，长句自动折行。
// 返回标签对象；换句时直接改文字即可（宽度与折行模式已经设好）。
lv_obj_t *ddj_passage_create(lv_obj_t *parent, int x, int y, int w, int h);

// 一段会折行的正文（点拨、参究问题都用它），左对齐、顶端起排。
lv_obj_t *ddj_paragraph_create(lv_obj_t *parent, int x, int y, int w, int h);

// 小字标签（章号、卷名、上次表态这类）。
lv_obj_t *ddj_note_create(lv_obj_t *parent, int x, int y, int w,
                          const char *text, uint32_t color);

// ---------------------------------------------------------------- 行 ------
#define DDJ_ROW_BORDER 3
#define DDJ_ROW_PAD    8
// 右侧状态小字的保留宽度。16px 字下「第一章」是 3 个字 48px，56 留出余量。
#define DDJ_ROW_NOTE_W 56
// 行高按「字库真实行高 + 上下边框」留：24px 那一档行高 29，加 3+3 边框是 35，
// 44 留出余量。首页要放下 4 行（4x44 + 3x6 间隔 = 194 < 248 的内容区），
// 再高就装不下了 —— 这个数字是被页面结构反推出来的，不是随手定的。
#define DDJ_ROW_H      44

ddj_row_t ddj_row_create(lv_obj_t *parent, int x, int y, int w, int h);
void ddj_row_update(ddj_row_t *row, const char *text, const char *note, ddj_state_t state);
