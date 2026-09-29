// main/qpq_page_reveal.c —— 判卷页：对错、正确答案、带答案的整句、解析、完整原文、出处。
//
// 网页版把这些内容内联展开在选项下方（同一屏内出现）；240x320 放不下四行候选
// 再加一段解析，所以这里独立成页，并且只有它可以滚动 —— 内容长度由内容决定，
// 与其砍掉出处与完整原文，不如让它滚。
#include "qpq_app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "qpq_content.h"
#include "qpq_player.h"
#include "qpq_ui.h"

// 左内缩与行间距。左内缩取 QPQ_BODY_X，与其它页面正文对齐。
#define REVEAL_PAD  QPQ_BODY_X
#define REVEAL_GAP  6

static lv_obj_t *s_hint;
static lv_obj_t *s_result;
static lv_obj_t *s_answer;
static lv_obj_t *s_sentence;
static lv_obj_t *s_explain;
static lv_obj_t *s_full;
static lv_obj_t *s_source;
static qpq_topbar_t s_bar;

// 在 flex 列里放一个可折行的标签。宽度用百分比而不是定值：列的左右内缩已经
// 由容器的 pad 给出，子项再写死宽度就会内缩两次。
static lv_obj_t *flex_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = qpq_label_create(parent, "", font, color);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(label, 4, 0);
    return label;
}

// 小标题：16px 灰墨，把三段内容分开。全用 24px 正文会让整页没有层次。
static lv_obj_t *caption_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = flex_label(parent, &qpq_font_16, QPQ_C_MUTED);
    lv_label_set_text(label, text);
    return label;
}

static void reveal_refresh(void)
{
    const qpq_session_t *session = qpq_app_session();
    const qpq_question_t *question = qpq_session_question(session);
    if (!question) return;

    const bool correct = qpq_session_is_correct(session);

    lv_label_set_text(s_result, correct ? "回答正确" : "回答错误");
    lv_obj_set_style_text_color(s_result, lv_color_hex(correct ? QPQ_C_GREEN
                                                              : QPQ_C_CINNABAR), 0);

    char answer[32];
    // 正确项就是 options[answer]：qpq_question_t 里只存下标，不存一份冗余的
    // 字符串，所以「正确答案」这件事只有一处定义。
    snprintf(answer, sizeof(answer), "正确答案：%s",
             question->options[question->answer]);
    lv_label_set_text(s_answer, answer);

    // 判卷页显示的是**已填空形态**（填空位换成正确答案），与答题页的槽位形态
    // 是两个不同长度，两种都在生成器与宿主测试里卡过。
    char filled[128];
    if (qpq_sentence_filled(question, filled, sizeof(filled)) > 0) {
        qpq_text_set(s_sentence, filled, QPQ_CHARS_BODY);
    } else {
        qpq_text_set(s_sentence, "", QPQ_CHARS_BODY);
    }
    qpq_text_set(s_explain, question->explain, QPQ_CHARS_BODY);
    qpq_text_set(s_full, question->full, QPQ_CHARS_BODY);
    qpq_text_set(s_source, question->source, QPQ_CHARS_SMALL);

    if (s_hint) {
        lv_label_set_text(s_hint, session->position + 1 >= session->run_length
                                      ? "确定 看成绩"
                                      : "确定 下一题");
    }
}

lv_obj_t *qpq_page_reveal_enter(void)
{
    s_hint = NULL;
    s_result = NULL;
    s_answer = NULL;
    s_sentence = NULL;
    s_explain = NULL;
    s_full = NULL;
    s_source = NULL;

    lv_obj_t *card = NULL;
    lv_obj_t *screen = qpq_page_create(&card);
    if (!card) return screen;

    s_bar = qpq_topbar_create(card, "", &qpq_font_16);
    s_hint = qpq_hint_create(card, "");
    lv_obj_t *body = qpq_body_create(card);

    lv_obj_t *scroll = qpq_scroll_create(body);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_left(scroll, REVEAL_PAD, 0);
    lv_obj_set_style_pad_right(scroll, REVEAL_PAD, 0);
    lv_obj_set_style_pad_row(scroll, REVEAL_GAP, 0);

    s_result = flex_label(scroll, &qpq_font_24, QPQ_C_INK);
    s_answer = flex_label(scroll, &qpq_font_24, QPQ_C_CINNABAR_D);
    s_sentence = flex_label(scroll, &qpq_font_24, QPQ_C_INK);

    caption_label(scroll, "解析");
    s_explain = flex_label(scroll, &qpq_font_24, QPQ_C_INK_SOFT);

    caption_label(scroll, "完整原文");
    s_full = flex_label(scroll, &qpq_font_24, QPQ_C_INK);

    caption_label(scroll, "出处");
    s_source = flex_label(scroll, &qpq_font_16, QPQ_C_INK_SOFT);

    reveal_refresh();
    return screen;
}

void qpq_page_reveal_leave(void)
{
    s_hint = NULL;
    s_result = NULL;
    s_answer = NULL;
    s_sentence = NULL;
    s_explain = NULL;
    s_full = NULL;
    s_source = NULL;
}

void qpq_page_reveal_key(qpq_key_t key)
{
    const qpq_session_t *session = qpq_app_session();
    const qpq_stage_t before = session->stage;
    const qpq_action_t action = qpq_session_key(qpq_app_session(), key);
    (void)before;

    switch (action) {
        case QPQ_ACT_ADVANCED:
            qpq_player_play_tone(QPQ_TONE_ENTER);
            qpq_app_goto_ask();
            return;

        case QPQ_ACT_FINISHED:
            qpq_player_play_tone(QPQ_TONE_COMPLETE);
            qpq_app_goto_summary();
            return;

        default:
            return;
    }
}
