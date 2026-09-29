// main/qpq_page_summary.c —— 结算页：评级、四项数据、一段评语。
//
// 评级与评语的门槛来自题库源文件（tools/qiaopi/bank.txt 所属的 content.py），
// 与网页版逐档一致：>=90 侨批大师，>=75 识字先贤，>=60 番客子弟，>=40 初识侨批，
// 否则需勤学。
#include "qpq_app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "qpq_content.h"
#include "qpq_player.h"
#include "qpq_ui.h"

#define SUMMARY_HEADLINE_Y 0
#define SUMMARY_HEADLINE_H 40
#define SUMMARY_SUBTITLE_Y 44
#define SUMMARY_STATS_Y    72
#define SUMMARY_STAT_STEP  22
#define SUMMARY_DESC_Y     168
#define SUMMARY_DESC_H     80

// 版式断言。这一页也是用满 248px 的一页，同样用编译期把「最后一项还在不在
// 正文区内、有没有互相重叠」钉住。
#define SUMMARY_STAT_COUNT 4
#define SUMMARY_NOTE_H     20   // 16px 字库的行高
#define SUMMARY_STATS_END \
    (SUMMARY_STATS_Y + (SUMMARY_STAT_COUNT - 1) * SUMMARY_STAT_STEP + SUMMARY_NOTE_H)
_Static_assert(SUMMARY_STATS_END <= SUMMARY_DESC_Y, "统计行与评语重叠");
_Static_assert(SUMMARY_DESC_Y + SUMMARY_DESC_H <= QPQ_BODY_H, "评语超出正文区，会被裁掉");
_Static_assert(SUMMARY_SUBTITLE_Y + SUMMARY_NOTE_H <= SUMMARY_STATS_Y, "副标题与统计行重叠");

static lv_obj_t *s_hint;
static lv_obj_t *s_rank;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_stat[SUMMARY_STAT_COUNT];
static lv_obj_t *s_desc;
static qpq_topbar_t s_bar;

static void summary_refresh(void)
{
    const qpq_session_t *session = qpq_app_session();
    const qpq_rank_t *rank = qpq_session_rank(session);

    lv_label_set_text(s_rank, rank->rank);
    lv_label_set_text(s_subtitle, rank->subtitle);

    char line[48];
    snprintf(line, sizeof(line), "正确 %u/%u",
             (unsigned)session->correct_count, (unsigned)session->run_length);
    lv_label_set_text(s_stat[0], line);
    snprintf(line, sizeof(line), "最高连对 %u 题", (unsigned)session->max_streak);
    lv_label_set_text(s_stat[1], line);
    snprintf(line, sizeof(line), "用时 %d 秒", qpq_app_elapsed_seconds());
    lv_label_set_text(s_stat[2], line);
    snprintf(line, sizeof(line), "得分 %d", session->score);
    lv_label_set_text(s_stat[3], line);

    // 评语最长 50 字，16px 每行 13 字 -> 4 行 = 80px，正好是分配给它的高度。
    qpq_text_set(s_desc, rank->description, QPQ_CHARS_SMALL);

    if (s_hint) lv_label_set_text(s_hint, "确定 再来一局 · 长按返回");
}

lv_obj_t *qpq_page_summary_enter(void)
{
    // 进结算页才把成绩落盘：中途放弃的局不该计入记录。这一句必须是本页唯一
    // 一次 commit，重复调用会把同一局数两遍。
    qpq_app_commit_run();

    s_hint = NULL;
    s_rank = NULL;
    s_subtitle = NULL;
    s_desc = NULL;
    for (int index = 0; index < SUMMARY_STAT_COUNT; index++) s_stat[index] = NULL;

    lv_obj_t *card = NULL;
    lv_obj_t *screen = qpq_page_create(&card);
    if (!card) return screen;

    s_bar = qpq_topbar_create(card, "本局成绩", &qpq_font_16);
    s_hint = qpq_hint_create(card, "");
    lv_obj_t *body = qpq_body_create(card);

    s_rank = qpq_headline_create(body, QPQ_BODY_X, SUMMARY_HEADLINE_Y, QPQ_BODY_W,
                                 SUMMARY_HEADLINE_H, QPQ_C_CINNABAR_D);
    s_subtitle = qpq_label_create(body, "", &qpq_font_16, QPQ_C_MUTED);
    lv_obj_set_pos(s_subtitle, QPQ_BODY_X, SUMMARY_SUBTITLE_Y);
    lv_obj_set_width(s_subtitle, QPQ_BODY_W);
    lv_obj_set_height(s_subtitle, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(s_subtitle, LV_TEXT_ALIGN_CENTER, 0);

    for (int index = 0; index < SUMMARY_STAT_COUNT; index++) {
        s_stat[index] = qpq_note_create(body, QPQ_BODY_X,
                                        SUMMARY_STATS_Y + index * SUMMARY_STAT_STEP,
                                        QPQ_BODY_W, "", QPQ_C_INK_SOFT);
    }

    s_desc = qpq_paragraph_create(body, QPQ_BODY_X, SUMMARY_DESC_Y, QPQ_BODY_W,
                                  SUMMARY_DESC_H, &qpq_font_16, QPQ_C_MUTED);

    summary_refresh();
    return screen;
}

void qpq_page_summary_leave(void)
{
    s_hint = NULL;
    s_rank = NULL;
    s_subtitle = NULL;
    s_desc = NULL;
    for (int index = 0; index < SUMMARY_STAT_COUNT; index++) s_stat[index] = NULL;
}

void qpq_page_summary_key(qpq_key_t key)
{
    const qpq_action_t action = qpq_session_key(qpq_app_session(), key);

    switch (action) {
        case QPQ_ACT_STARTED:
            // 再来一局：背景音乐从头放，计时重新起算。
            qpq_player_play_tone(QPQ_TONE_ENTER);
            qpq_app_note_run_started();
            qpq_player_start_bgm();
            qpq_app_goto_ask();
            return;

        case QPQ_ACT_LEFT:
            qpq_player_play_tone(QPQ_TONE_BACK);
            qpq_player_stop_all();
            qpq_app_goto_title();
            return;

        default:
            return;
    }
}
