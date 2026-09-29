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

// ------------------------------------------------------- 每行字数预算 -----
// 「一屏放得下」不是感觉，是一道除法：内容区宽 210px，同一字号下汉字等宽
// （Noto Sans CJK 的全角字形，前进宽度就等于字号），所以每行几个字 = 除法取整。
// 这三个数字必须与 tools/daodejing/content.py 顶部那张表一致 —— 那里用它卡
// 内容长度，这里用它排版；两边一旦不一致，就会出现「生成阶段过了、屏上却溢了」。
// ddj_ui.c 用 _Static_assert 把「预算 x 字号 <= 内容区宽」钉在编译期。
#define DDJ_CHARS_PASSAGE (DDJ_BODY_W / 32)   // 6
#define DDJ_CHARS_PARA    (DDJ_BODY_W / 24)   // 8
#define DDJ_CHARS_NOTE    (DDJ_BODY_W / 16)   // 13

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

// 把一段中文折行后写进标签。折行缓冲是模块内的静态区，一次调用写完就拷进
// 标签，所以调用方不用自己开缓冲区，也不用管它的生命周期。
void ddj_text_set(lv_obj_t *label, const char *text, int chars_per_line);

// 小字标签（章号、卷名、上次表态这类）。
lv_obj_t *ddj_note_create(lv_obj_t *parent, int x, int y, int w,
                          const char *text, uint32_t color);

// ---------------------------------------------------------------- 行 ------
#define DDJ_ROW_BORDER 3
#define DDJ_ROW_PAD    8
// 右侧状态小字的保留宽度。16px 字下「第八十一章」是 5 个字 80px，88 留出余量 ——
// 章号一律用中文数字，与顶栏、正文档位保持一致，不为了省宽度换成阿拉伯数字。
#define DDJ_ROW_NOTE_W 88
// 行高按「字库真实行高 + 上下边框」留：24px 那一档行高 29，加 3+3 边框是 35，
// 44 留出余量。首页要放下 4 行（4x44 + 3x6 间隔 = 194 < 248 的内容区），
// 再高就装不下了 —— 这个数字是被页面结构反推出来的，不是随手定的。
#define DDJ_ROW_H      44

// note_width 为 0 表示这一行没有右侧状态，主文字用满整行。
ddj_row_t ddj_row_create(lv_obj_t *parent, int x, int y, int w, int h, int note_width);
void ddj_row_update(ddj_row_t *row, const char *text, const char *note, ddj_state_t state);

// 只改状态，不动文字。
//
// 为什么不复用 ddj_row_update：那需要把当前文字再传一遍，而「取当前文字」只有
// lv_label_get_text 一条路 —— 它返回的是标签内部的缓冲区，而 lv_label_set_text
// 会先 free 旧文本再复制，于是「把那个指针喂回同一个标签」就是 use-after-free。
// 与其在调用处小心规避，不如在这里开一个只碰状态的入口。
void ddj_row_set_state(ddj_row_t *row, ddj_state_t state);

// --------------------------------------------------------------- 列表 -----
// 纵向可滚动列表。章节目录与「待参」两页共用一套：都是「若干行 + 一个光标 +
// 光标移出视野时滚过去」，不必写两遍。
//
// 容量按全本 81 章写死（ddj_ui.c 里有 _Static_assert 与 DDJ_TOTAL_CHAPTERS 对齐）：
// 这台机器没有 PSRAM，列表按上限静态分配，不做运行时扩容 —— 81 行 x 24 字节
// 不到 2KB，比一条会失败的 malloc 路径便宜得多。
#define DDJ_LIST_CAPACITY 81

typedef struct {
    lv_obj_t *list;   // 滚动容器
    ddj_row_t rows[DDJ_LIST_CAPACITY];
    int count;
    int sel;
} ddj_list_t;

// 在 body 上铺一个纵向列表并建 count 行（行内容留空，由调用方用
// ddj_list_row 取出后填充）。count 会被夹到 [0, DDJ_LIST_CAPACITY]。
void ddj_list_build(ddj_list_t *list, lv_obj_t *body, int count, int note_width);

// 第 index 行；越界返回 NULL。
ddj_row_t *ddj_list_row(ddj_list_t *list, int index);

// 把光标放到 index（越界则忽略），刷新行的选中态并滚动到可见。
void ddj_list_select(ddj_list_t *list, int index);

// 光标上下移动，返回 true 表示光标真的动了（用来决定要不要响一声）。
bool ddj_list_move(ddj_list_t *list, int delta);

// 清空一页的状态（leave 时用），只清结构体，不碰 LVGL 对象。
void ddj_list_clear(ddj_list_t *list);

