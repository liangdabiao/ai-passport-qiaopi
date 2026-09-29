// main/szj_ui.h —— 三字经游戏自己的界面层:主题、字体与可复用控件。
//
// 本文件与基线 demo 的 ui_pixel.* 外壳无关。三字经游戏按自己的需要重新设计了
// 版式与控件(顶栏 + 内容区 + 底栏提示、列表行、选项行、田字格),只复用 BSP
// 提供的显示/按键/电量接口和 LVGL 通用能力。
//
// 版式:240x320 的屏,四角有 30px 圆角遮罩。因此逻辑屏幕底色取淡墨(与遮罩的
// 纯黑过渡自然),真正的页面是一张四周内缩 5px、圆角 25px 的「宣纸」卡片 ——
// 半径 25 < 遮罩半径 30,卡片本身就完整落在可见区里,不必猜边界。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

// ---------------------------------------------------------------- 配色 -----
#define SZJ_C_PAPER      0xFBF3E0   // 页面底:宣纸
#define SZJ_C_PAPER_ALT  0xF2E5C8   // 次级底:加深的纸色(未选中的行)
#define SZJ_C_INK        0x2B2016   // 淡墨:正文与顶/底栏
#define SZJ_C_INK_SOFT   0x6B5A45   // 次级文字
#define SZJ_C_MUTED      0xA08E74   // 提示文字
#define SZJ_C_LINE       0xDCC9A0   // 行边框、田字格
#define SZJ_C_RED        0xC0392B   // 朱红:强调与选中
#define SZJ_C_RED_DARK   0x8E2A20
#define SZJ_C_RED_SOFT   0xF8DAD5   // 答错的行底
#define SZJ_C_GOLD       0xE8B44A   // 藤黄
#define SZJ_C_GOLD_SOFT  0xF7E7BC   // 选中的行底
#define SZJ_C_BAMBOO     0x6F9E4A   // 竹青:答对
#define SZJ_C_BAMBOO_INK 0x2F5D1E
#define SZJ_C_BAMBOO_SOFT 0xE2F0CE  // 答对的行底
#define SZJ_C_HINT       0xD8C7AC   // 底栏提示文字

// --------------------------------------------------------------- 版式 ------
#define SZJ_SCREEN_W     240
#define SZJ_SCREEN_H     320
#define SZJ_PAGE_INSET   5
#define SZJ_PAGE_W       (SZJ_SCREEN_W - 2 * SZJ_PAGE_INSET)          // 230
#define SZJ_PAGE_H       (SZJ_SCREEN_H - 2 * SZJ_PAGE_INSET)          // 310
#define SZJ_PAGE_RADIUS  25
#define SZJ_BAR_H        36
#define SZJ_HINT_H       26
#define SZJ_BODY_TOP     SZJ_BAR_H                                    // 36
#define SZJ_BODY_BOTTOM  (SZJ_PAGE_H - SZJ_HINT_H)                    // 284
#define SZJ_BODY_H       (SZJ_BODY_BOTTOM - SZJ_BODY_TOP)             // 248
#define SZJ_BODY_X       10
#define SZJ_BODY_W       (SZJ_PAGE_W - 2 * SZJ_BODY_X)                // 210

// --------------------------------------------------------------- 字体 ------
// 由 tools/sanzijing/gen_font.py 生成到 assets/fonts/,覆盖经文全部用字、
// 界面文案用字、可打印 ASCII 与界面用到的符号。没有 fallback:任何界面文案
// 里的字符都会被生成器扫进字库,漏字会在生成阶段报错而不是在屏上变空白框。
LV_FONT_DECLARE(szj_font_16);
LV_FONT_DECLARE(szj_font_24);
LV_FONT_DECLARE(szj_font_32);

// ------------------------------------------------------------- 控件状态 ----
typedef enum {
    SZJ_STATE_NORMAL = 0,
    SZJ_STATE_SELECTED,
    SZJ_STATE_CORRECT,
    SZJ_STATE_WRONG,
} szj_state_t;

typedef enum {
    SZJ_DOT_PENDING = 0,
    SZJ_DOT_CURRENT,
    SZJ_DOT_CORRECT,
    SZJ_DOT_WRONG,
} szj_dot_state_t;

// 列表行:左侧 24px 主文字 + 右侧 16px 状态小字。
typedef struct {
    lv_obj_t *box;
    lv_obj_t *text;
    lv_obj_t *status;   // 选项行不需要,为 NULL
} szj_row_t;

// ------------------------------------------------------------- 页面骨架 ----
// 建逻辑屏幕(淡墨底)与页面卡(宣纸)。返回屏幕根对象,供 lv_screen_load /
// lv_obj_delete 使用;*out_card 回填页面卡,后续控件都挂在它上面。
lv_obj_t *szj_page_create(lv_obj_t **out_card);

// 内容区容器:夹在顶栏与底栏之间的一块透明区域,子控件坐标从它的左上角起算。
lv_obj_t *szj_body_create(lv_obj_t *card);

// 顶栏(淡墨底):左侧标题,右侧电量。电量读不到时显示 "--" 而不是编一个数。
void szj_topbar_create(lv_obj_t *card, const char *title);

// 底栏提示条(淡墨底)。返回其中的文字标签,便于页面刷新读数。
lv_obj_t *szj_hint_create(lv_obj_t *card, const char *text);

// ------------------------------------------------------------- 基础控件 ----
// 圆角面板。border 为 0 时不画边框。
lv_obj_t *szj_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t bg, uint32_t border, int border_w);

lv_obj_t *szj_label_create(lv_obj_t *parent, const char *text,
                           const lv_font_t *font, uint32_t color);

// 竹简小图标(首页装饰,48x46)。
lv_obj_t *szj_scroll_emblem_create(lv_obj_t *parent, int x, int y);

// 田字格底线:一个方框加虚线十字。
lv_obj_t *szj_grid_create(lv_obj_t *parent, int x, int y, int size, uint32_t color);

// 小题进度点。
lv_obj_t *szj_dot_create(lv_obj_t *parent, int x, int y, int size);
void szj_dot_set(lv_obj_t *dot, szj_dot_state_t state);

// ------------------------------------------------------------- 行与选项 ----
szj_row_t szj_row_create(lv_obj_t *parent, int x, int y, int w, int h);
void szj_row_update(szj_row_t *row, const char *text, const char *status,
                    szj_state_t state);

szj_row_t szj_option_create(lv_obj_t *parent, int x, int y, int w, int h);
void szj_option_update(szj_row_t *row, const char *text, szj_state_t state);

// --------------------------------------------------------------- 文本 ------
// 把星星写成一串 ★/☆,如 "★★☆"。cap 为满星数(每课 3)。
#define SZJ_STARS_TEXT_CAPACITY 32
void szj_stars_text(int stars, int cap, char *out, size_t capacity);
