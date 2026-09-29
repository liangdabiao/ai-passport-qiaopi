// main/qpq_page_ask.c —— 答题页：一句带空格的侨批、四个候选（甲乙丙丁）。
//
// 版式（相对页面卡）：
//   顶栏 36 / 句子 36..123（3 行 x 29） / 四个候选 128..284 / 底栏 284..310
// 句子固定占 3 行的高度，即使这一题只有两行也不上移 —— 否则每翻一题整页都在跳，
// 眼睛要重新找位置。四行候选是 4x36 + 3x4 = 156px，加上句子 87px 与间隔 5px
// 正好 248px，用满内容区。
#include "qpq_app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "qpq_content.h"
#include "qpq_player.h"
#include "qpq_ui.h"

#define ASK_SENTENCE_Y 0
#define ASK_SENTENCE_H 87
#define ASK_OPTION_Y   92

// 版式断言。这一页的纵向预算用得最满（正文区 248px 里用掉 248px，余量为零），所以
// 「最后一行候选还在不在正文区内」必须由编译器保证 —— 判卷页的滚动问题、标题页
// 成绩行被裁掉，都是同一类错：算错一个 y 值，界面上不报错，只是有东西看不见。
#define ASK_OPTION_END \
    (ASK_OPTION_Y + (QPQ_OPTION_COUNT - 1) * (QPQ_ROW_H + QPQ_ROW_GAP) + QPQ_ROW_H)
_Static_assert(ASK_OPTION_END <= QPQ_BODY_H, "最后一行候选超出正文区，会被裁掉");
_Static_assert(ASK_SENTENCE_Y + ASK_SENTENCE_H <= ASK_OPTION_Y, "句子与候选行重叠");

static lv_obj_t *s_hint;
static lv_obj_t *s_sentence;
static qpq_topbar_t s_bar;
static qpq_row_t s_options[QPQ_OPTION_COUNT];
static char s_slot[128];

static void ask_paint_options(void)
{
    const qpq_session_t *session = qpq_app_session();
    const qpq_question_t *question = qpq_session_question(session);
    if (!question) return;

    for (uint8_t index = 0; index < (uint8_t)QPQ_OPTION_COUNT; index++) {
        qpq_row_update(&s_options[index], qpq_answer_slot(index),
                       question->options[index], "",
                       index == session->cursor ? QPQ_STATE_SELECTED : QPQ_STATE_NORMAL);
    }
}

static void ask_refresh(void)
{
    const qpq_session_t *session = qpq_app_session();
    const qpq_question_t *question = qpq_session_question(session);
    if (!question) return;

    if (s_sentence) {
        // 答题页在选之前只能显示槽位形态，它的字符数与源句相同（见 qpq_content）。
        const uint16_t written = qpq_sentence_slot(question, s_slot, sizeof(s_slot));
        qpq_text_set(s_sentence, written ? s_slot : "", QPQ_CHARS_BODY);
    }

    qpq_topbar_set_left(&s_bar, question->category);
    char progress[16];
    snprintf(progress, sizeof(progress), "%u/%u",
             (unsigned)(session->position + 1), (unsigned)session->run_length);
    qpq_topbar_set_right(&s_bar, progress);

    ask_paint_options();

    if (s_hint) lv_label_set_text(s_hint, "上下选择 · 确定作答");
}

lv_obj_t *qpq_page_ask_enter(void)
{
    s_hint = NULL;
    s_sentence = NULL;
    for (uint8_t index = 0; index < (uint8_t)QPQ_OPTION_COUNT; index++) {
        s_options[index] = (qpq_row_t){0};
    }

    lv_obj_t *card = NULL;
    lv_obj_t *screen = qpq_page_create(&card);
    if (!card) return screen;

    s_bar = qpq_topbar_create(card, "", &qpq_font_16);
    s_hint = qpq_hint_create(card, "");
    lv_obj_t *body = qpq_body_create(card);

    // 句子用 24px 居中。不用 32px：四行候选占掉 156px 之后只余 87px，
    // 32px 只能放 2 行，长句会被截。
    s_sentence = qpq_headline_create(body, QPQ_BODY_X, ASK_SENTENCE_Y, QPQ_BODY_W,
                                     ASK_SENTENCE_H, QPQ_C_INK);
    lv_obj_set_style_text_font(s_sentence, &qpq_font_24, 0);

    for (uint8_t index = 0; index < (uint8_t)QPQ_OPTION_COUNT; index++) {
        const int y = ASK_OPTION_Y + index * (QPQ_ROW_H + QPQ_ROW_GAP);
        s_options[index] = qpq_row_create(body, QPQ_BODY_X, y, QPQ_BODY_W, QPQ_ROW_H,
                                          QPQ_ROW_TAG_W, 0);
    }

    ask_refresh();
    return screen;
}

void qpq_page_ask_leave(void)
{
    s_hint = NULL;
    s_sentence = NULL;
    for (uint8_t index = 0; index < (uint8_t)QPQ_OPTION_COUNT; index++) {
        s_options[index] = (qpq_row_t){0};
    }
}

void qpq_page_ask_key(qpq_key_t key)
{
    qpq_session_t *session = qpq_app_session();
    const qpq_action_t action = qpq_session_key(session, key);

    switch (action) {
        case QPQ_ACT_MOVED:
            qpq_player_play_tone(QPQ_TONE_MOVE);
            ask_paint_options();
            return;

        case QPQ_ACT_ANSWERED: {
            // 判卷音：网页版在这一刻响一声对／错，设备多给一个「对」的上行终止。
            qpq_player_play_tone(qpq_session_is_correct(session) ? QPQ_TONE_CORRECT
                                                                : QPQ_TONE_WRONG);
            // 方言配音读的是**正确答案的整句**，所以它在作答之后才播 ——
            // 与网页版同一时机，提前放等于泄题。
            const uint16_t clip = qpq_session_narration_clip(session);
            if (clip != UINT16_MAX) qpq_player_play_voice(clip);
            qpq_app_goto_reveal();
            return;
        }

        case QPQ_ACT_LEFT:
            // 放弃本局：把音乐一起停掉，回标题页保持安静。
            qpq_player_play_tone(QPQ_TONE_BACK);
            qpq_player_stop_all();
            qpq_app_goto_title();
            return;

        default:
            return;
    }
}
