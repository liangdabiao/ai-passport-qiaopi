// main/szj_card.c —— 认字卡:一句一卡。
//
// 三格田字格各放一个字,下面两行摆出整课的十二个字,当前这句用朱红标出来 ——
// 这样孩子看到的不只是孤立的三个字,而是它在整课里的位置。
//
// 上/下 逐句翻;确定 一次跳过一整课,否则 404 句要点 404 下。
// 读到第几句记进进度,下次进来接着看。
#include "szj_card.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "szj_progress.h"
#include "szj_sound.h"
#include "szj_store.h"
#include "szj_text.h"
#include "szj_text_util.h"
#include "szj_ui.h"

#define CARD_CELL      70
#define CARD_CELL_GAP  4
#define CARD_GRID_Y    40
#define CARD_HALF_Y0   126
#define CARD_HALF_Y1   170
#define CARD_GLYPH_BYTES 3   // 一个汉字 3 字节
#define CARD_CELL_COUNT  3   // 每句三个字,正好放三格田字格
// 一整课 4 句 = 12 字,拆成上下两行,每行 2 句 = 6 字 = 18 字节。
// 这里必须自己算清楚字节数:两行加起来要正好覆盖整课 36 字节,
// 少了就会把最后一句悄悄吞掉(界面测试跑不到,只有真机才看得出来)。
#define CARD_HALF_LINES 2
#define CARD_HALF_BYTES (CARD_GLYPH_BYTES * CARD_CELL_COUNT * CARD_HALF_LINES)
#define CARD_HALF_COUNT (SZJ_LINES_PER_STANZA / CARD_HALF_LINES)

// 两行必须正好铺满一整课,多一个字节少一个字节都是错的 —— 少了会吞掉尾句,
// 多了会读到下一课的头上。这行断言是这句话的机器可验版本。
_Static_assert(CARD_HALF_BYTES * CARD_HALF_COUNT ==
                   (SZJ_LINE_BYTES - 1) * SZJ_LINES_PER_STANZA,
               "认字卡两行必须正好覆盖一整课的字节数");

static lv_obj_t *s_body;
static lv_obj_t *s_hint;
static lv_obj_t *s_position;
static lv_obj_t *s_lesson;
static lv_obj_t *s_glyph[CARD_CELL_COUNT];
static lv_obj_t *s_stanza[CARD_HALF_COUNT];
static int s_line;
static bool s_dirty;

static int card_clamp(int line)
{
    if (line < 0) return 0;
    if (line >= SZJ_LINE_COUNT) return SZJ_LINE_COUNT - 1;
    return line;
}

static void card_refresh(void)
{
    const char *text = szj_line(s_line);
    if (!text) return;

    const size_t length = strlen(text);
    char glyph[CARD_GLYPH_BYTES + 1];
    for (int i = 0; i < CARD_CELL_COUNT; i++) {
        const size_t offset = (size_t)i * CARD_GLYPH_BYTES;
        if (offset + CARD_GLYPH_BYTES <= length) {
            memcpy(glyph, text + offset, CARD_GLYPH_BYTES);
            glyph[CARD_GLYPH_BYTES] = '\0';
        } else {
            glyph[0] = '\0';
        }
        if (s_glyph[i]) lv_label_set_text(s_glyph[i], glyph);
    }

    const int stanza = szj_stanza_of_line(s_line);
    if (s_position) {
        // 同 szj_lesson.c:缓冲区按 int 的最坏十进制宽度留,不按实际句数留。
        char position[48];
        snprintf(position, sizeof(position), "第 %d/%d 句", s_line + 1, SZJ_LINE_COUNT);
        lv_label_set_text(s_position, position);
    }
    if (s_lesson) {
        char lesson[48];
        snprintf(lesson, sizeof(lesson), "第 %d 课", stanza + 1);
        lv_label_set_text(s_lesson, lesson);
    }

    char whole[SZJ_STANZA_TEXT_CAPACITY];
    if (szj_stanza_text(stanza, whole, sizeof(whole)) == 0) whole[0] = '\0';

    // 整课 12 字拆成两行,每行 2 句。当前这句落在哪一行,哪一行就用朱红。
    const int active_half = (s_line % SZJ_LINES_PER_STANZA) / CARD_HALF_LINES;
    for (int i = 0; i < CARD_HALF_COUNT; i++) {
        if (!s_stanza[i]) continue;
        char half[CARD_HALF_BYTES + 1];
        memcpy(half, whole + (size_t)i * CARD_HALF_BYTES, CARD_HALF_BYTES);
        half[CARD_HALF_BYTES] = '\0';
        lv_label_set_text(s_stanza[i], half);
        lv_obj_set_style_text_color(s_stanza[i],
            lv_color_hex(i == active_half ? SZJ_C_RED_DARK : SZJ_C_MUTED), 0);
    }
}

static void card_sync(void)
{
    szj_progress_t *progress = szj_store_progress();
    if (!progress) return;
    const uint16_t line = (uint16_t)s_line;
    if (line != progress->card_line) {
        progress->card_line = line;
        s_dirty = true;
    }
}

lv_obj_t *szj_card_enter(void)
{
    const szj_progress_t *progress = szj_store_progress();
    s_line = card_clamp(progress ? (int)progress->card_line : 0);
    s_dirty = false;
    s_body = NULL;
    s_hint = NULL;
    s_position = NULL;
    s_lesson = NULL;
    for (int i = 0; i < CARD_CELL_COUNT; i++) s_glyph[i] = NULL;
    for (int i = 0; i < CARD_HALF_COUNT; i++) s_stanza[i] = NULL;

    lv_obj_t *card = NULL;
    lv_obj_t *scr = szj_page_create(&card);
    if (!card) return scr;

    szj_topbar_create(card, "认字卡");
    s_hint = szj_hint_create(card, "上/下 翻卡 · 确定 下一课");
    s_body = szj_body_create(card);

    s_position = szj_label_create(s_body, "", &szj_font_16, SZJ_C_RED);
    lv_obj_set_pos(s_position, SZJ_BODY_X, 6);
    s_lesson = szj_label_create(s_body, "", &szj_font_16, SZJ_C_MUTED);
    lv_obj_align(s_lesson, LV_ALIGN_TOP_RIGHT, -SZJ_BODY_X, 6);

    const int grid_w = CARD_CELL_COUNT * CARD_CELL + (CARD_CELL_COUNT - 1) * CARD_CELL_GAP;
    const int start_x = (SZJ_PAGE_W - grid_w) / 2;
    for (int i = 0; i < CARD_CELL_COUNT; i++) {
        lv_obj_t *grid = szj_grid_create(s_body,
                                         start_x + i * (CARD_CELL + CARD_CELL_GAP),
                                         CARD_GRID_Y, CARD_CELL, SZJ_C_LINE);
        s_glyph[i] = szj_label_create(grid, "", &szj_font_32, SZJ_C_INK);
        if (s_glyph[i]) lv_obj_center(s_glyph[i]);
    }

    static const int CARD_HALF_Y[CARD_HALF_COUNT] = { CARD_HALF_Y0, CARD_HALF_Y1 };
    for (int i = 0; i < CARD_HALF_COUNT; i++) {
        s_stanza[i] = szj_label_create(s_body, "", &szj_font_24, SZJ_C_MUTED);
        if (s_stanza[i]) {
            lv_obj_align(s_stanza[i], LV_ALIGN_TOP_MID, 0, CARD_HALF_Y[i]);
        }
    }

    card_refresh();
    return scr;
}

void szj_card_leave(void)
{
    if (s_dirty) {
        szj_store_save();
        s_dirty = false;
    }
    s_body = NULL;
    s_hint = NULL;
    s_position = NULL;
    s_lesson = NULL;
    for (int i = 0; i < CARD_CELL_COUNT; i++) s_glyph[i] = NULL;
    for (int i = 0; i < CARD_HALF_COUNT; i++) s_stanza[i] = NULL;
}

void szj_card_key(szj_key_t key)
{
    switch (key) {
        case SZJ_KEY_UP:
            if (s_line == 0) return;
            s_line--;
            szj_sound_play(SZJ_SOUND_MOVE);
            card_refresh();
            card_sync();
            break;
        case SZJ_KEY_DOWN:
            if (s_line + 1 >= SZJ_LINE_COUNT) return;
            s_line++;
            szj_sound_play(SZJ_SOUND_MOVE);
            card_refresh();
            card_sync();
            break;
        case SZJ_KEY_OK: {
            const int next = (szj_stanza_of_line(s_line) + 1) * SZJ_LINES_PER_STANZA;
            if (next >= SZJ_LINE_COUNT) {
                // 已经是最后一课,没有"下一课"可去。
                szj_sound_play(SZJ_SOUND_WRONG);
                return;
            }
            s_line = next;
            szj_sound_play(SZJ_SOUND_ENTER);
            card_refresh();
            card_sync();
            break;
        }
        case SZJ_KEY_BACK:
            szj_sound_play(SZJ_SOUND_BACK);
            szj_app_goto_home();
            break;
        default:
            break;
    }
}
