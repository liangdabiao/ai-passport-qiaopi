// main/qpq_page_title.c —— 标题页：一个主标题、三项菜单、一行成绩。
//
// 只有三项，第一项就是「开始一局」—— 打开机器按两下就能开始，不需要先翻目录。
// 三个键的菜单项数越多越容易迷失。
#include "qpq_app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "qpq_content.h"
#include "qpq_player.h"
#include "qpq_store.h"
#include "qpq_ui.h"

#define TITLE_ITEMS 3

static const char *const TITLE_LABEL[TITLE_ITEMS] = {
    "开始一局",
    "音量",
    "重置记录",
};

// 三项菜单里右侧小字要放得下「已重置」这种四字提示。
#define TITLE_NOTE_W 72
// 版式（相对页面卡，正文区高 QPQ_BODY_H = 248）：顶栏 0..36 / 大字 42..82 /
// 成绩 88..108 / 菜单 124..240 / 底栏 284..310
//
// 成绩行原来在 250 —— **超出正文区底部 2px，整行被 LVGL 裁掉，在真机上是不可见的**。
// 当时没人发现是因为它只是「已见 x/91 · 最好 y 分」这么一行小字，缺了不会让人起疑。
// 现在它接替了原来那句与顶栏重复的副标题（「民国侨批 · 填字问答」），于是既回到
// 正文区内，又没有减少任何信息。下面两条断言把这个结论钉在编译期。
#define TITLE_HEADLINE_Y 42
#define TITLE_HEADLINE_H  40
#define TITLE_STATS_Y     88
#define TITLE_STATS_H     20   // 16px 字库的行高
#define TITLE_MENU_Y      124

#define TITLE_MENU_END \
    (TITLE_MENU_Y + (TITLE_ITEMS - 1) * (QPQ_ROW_H + QPQ_ROW_GAP) + QPQ_ROW_H)
_Static_assert(TITLE_MENU_END <= QPQ_BODY_H, "菜单最后一行超出正文区，会被裁掉");
_Static_assert(TITLE_STATS_Y + TITLE_STATS_H <= QPQ_BODY_H, "成绩行超出正文区，会被裁掉");
_Static_assert(TITLE_STATS_Y + TITLE_STATS_H <= TITLE_MENU_Y, "成绩行会与菜单重叠");

static qpq_row_t s_rows[TITLE_ITEMS];
static lv_obj_t *s_hint;
static lv_obj_t *s_stats;
static int s_sel;
// 重置记录要按两次：第一次「上膛」，第二次才真的清。在只有三个键的机器上，
// 一步就把记录抹掉太容易误触。
static bool s_confirm_reset;

static void title_refresh(void)
{
    const qpq_progress_t *progress = qpq_app_progress();

    char notes[TITLE_ITEMS][16];
    notes[0][0] = '\0';
    // 音量档位：0 档的文本就是「关」，所以这一项同时承担了原来「音效开关」的职责
    // —— 三个键不值得两个设置项。档位文本最长是 "100%"（5 字节含结尾），每格 16
    // 字节，不可能写不下。
    (void)qpq_app_volume_text(notes[1], sizeof(notes[1]));
    snprintf(notes[2], sizeof(notes[2]), "%s", s_confirm_reset ? "再按一次" : "");

    for (int index = 0; index < TITLE_ITEMS; index++) {
        // 菜单行没有左侧标记（建行时 tag_width 给的是 0），所以 tag 传 NULL。
        qpq_row_update(&s_rows[index], NULL, TITLE_LABEL[index], notes[index],
                       index == s_sel ? QPQ_STATE_SELECTED : QPQ_STATE_NORMAL);
    }

    if (s_stats && progress) {
        char stats[64];
        // 两种形态都按真实值写，但缓冲按最坏情况开：%d 最宽 11 位，
        // 「已见」+「最好」+「分」+ 两个分隔符一共 9 个字 = 27 字节，
        // 加上四个数字 44 字节，再加结尾，64 刚好够；这里再留一位余量。
        snprintf(stats, sizeof(stats),
                 "已见 %u/%u · 最好 %d 分",
                 (unsigned)qpq_progress_seen_count(progress),
                 (unsigned)qpq_question_count(),
                 (int)progress->best_score);
        lv_label_set_text(s_stats, stats);
    }

    if (s_hint) {
        if (s_confirm_reset) {
            lv_label_set_text(s_hint, "再按一次确定即清空记录");
        } else if (s_sel == 1) {
            lv_label_set_text(s_hint, "确定 切换音量档位");
        } else {
            lv_label_set_text(s_hint, "上下选择 · 确定进入");
        }
    }
}

static void title_activate(void)
{
    switch (s_sel) {
        case 0:
            qpq_player_play_tone(QPQ_TONE_ENTER);
            qpq_app_note_run_started();
            if (qpq_app_session()->stage == QPQ_STAGE_TITLE) {
                qpq_session_start(qpq_app_session());
            }
            qpq_player_start_bgm();
            qpq_app_goto_ask();
            return;

        case 1:
            // 先改档位、再响提示音：从「关」往上调的那一次，提示音必须按**新**档位
            // 响出来。顺序反过来会静默 —— 关档时音效被直接丢弃，用户按了确定什么
            // 都没听到，会以为按键坏了或音量没调上去。
            qpq_app_cycle_volume();
            qpq_player_play_tone(QPQ_TONE_ENTER);
            title_refresh();
            return;

        case 2:
            if (!s_confirm_reset) {
                s_confirm_reset = true;
                qpq_player_play_tone(QPQ_TONE_ENTER);
                title_refresh();
                return;
            }
            qpq_player_play_tone(QPQ_TONE_BACK);
            qpq_app_reset_progress();
            s_confirm_reset = false;
            title_refresh();
            return;

        default:
            return;
    }
}

lv_obj_t *qpq_page_title_enter(void)
{
    s_sel = 0;
    s_confirm_reset = false;
    s_hint = NULL;
    s_stats = NULL;
    for (int index = 0; index < TITLE_ITEMS; index++) s_rows[index] = (qpq_row_t){0};

    lv_obj_t *card = NULL;
    lv_obj_t *screen = qpq_page_create(&card);
    if (!card) return screen;

    qpq_topbar_t bar = qpq_topbar_create(card, "侨批 · 填字问答", &qpq_font_16);
    qpq_topbar_add_battery(&bar);
    s_hint = qpq_hint_create(card, "");
    lv_obj_t *body = qpq_body_create(card);

    lv_obj_t *headline = qpq_headline_create(body, QPQ_BODY_X, TITLE_HEADLINE_Y,
                                             QPQ_BODY_W, TITLE_HEADLINE_H, QPQ_C_INK);
    lv_label_set_text(headline, "侨批");

    // 这一行原来是副标题「民国侨批 · 填字问答」，与顶栏那句几乎重复；让位给成绩。
    // 位置没变，变的是内容更有用 —— 也顺手修掉了它以前被放在正文区之外、
    // 在真机上根本看不见的问题（见上面的版式注释与断言）。
    s_stats = qpq_note_create(body, QPQ_BODY_X, TITLE_STATS_Y, QPQ_BODY_W, "",
                              QPQ_C_MUTED);

    for (int index = 0; index < TITLE_ITEMS; index++) {
        const int y = TITLE_MENU_Y + index * (QPQ_ROW_H + QPQ_ROW_GAP);
        s_rows[index] = qpq_row_create(body, QPQ_BODY_X, y, QPQ_BODY_W, QPQ_ROW_H,
                                       0, TITLE_NOTE_W);
    }

    title_refresh();
    return screen;
}

void qpq_page_title_leave(void)
{
    s_hint = NULL;
    s_stats = NULL;
    s_confirm_reset = false;
    for (int index = 0; index < TITLE_ITEMS; index++) s_rows[index] = (qpq_row_t){0};
}

void qpq_page_title_key(qpq_key_t key)
{
    switch (key) {
        case QPQ_KEY_UP:
            if (s_sel == 0) return;
            s_sel--;
            s_confirm_reset = false;
            qpq_player_play_tone(QPQ_TONE_MOVE);
            title_refresh();
            return;

        case QPQ_KEY_DOWN:
            if (s_sel + 1 >= TITLE_ITEMS) return;
            s_sel++;
            s_confirm_reset = false;
            qpq_player_play_tone(QPQ_TONE_MOVE);
            title_refresh();
            return;

        case QPQ_KEY_OK:
            title_activate();
            return;

        case QPQ_KEY_BACK:
            // 标题页是根，长按什么也不做 —— 在一块不能关机的板子上，
            // 「退出应用」没有意义。
            if (s_confirm_reset) {
                s_confirm_reset = false;
                title_refresh();
            }
            return;

        default:
            return;
    }
}
