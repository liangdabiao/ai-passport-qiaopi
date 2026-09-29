// main/qpq_page_reveal.c —— 判卷页：对错、正确答案、带答案的整句、解析、完整原文、出处。
//
// 网页版把这些内容内联展开在选项下方（同一屏内出现）；240x320 放不下四行候选
// 再加一段解析，所以这里独立成页，并且只有它可以滚动 —— 内容长度由内容决定，
// 与其砍掉出处与完整原文，不如让它滚。
//
// **滚这件事必须有键能操作。** 这一页刚做出来时，上下键被原样转给状态机，而状态机
// 在判卷阶段只认确定键 —— 于是内容超出一屏、界面上却什么也做不了，后半段（解析、
// 完整原文、出处）在真机上根本看不到。现在上下键由本页自己消费（见
// qpq_page_reveal_key），并且提示条会明说「上下翻阅 / 已到底」。
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

// 一次翻页的像素数。取 24px 字库的「行高 29 + 行距 4」= 33，这样翻完之后每一行的
// 位置与翻之前一样整齐；随手取 40 会让文字永远停在半行上，读起来很累。
#define REVEAL_SCROLL_STEP 33

static lv_obj_t *s_hint;
static lv_obj_t *s_result;
static lv_obj_t *s_answer;
static lv_obj_t *s_sentence;
static lv_obj_t *s_explain;
static lv_obj_t *s_full;
static lv_obj_t *s_source;
static qpq_topbar_t s_bar;
// 本页唯一的可滚动容器。**上下键必须由这一页自己处理**：状态机在判卷阶段只认
// 确定键（上/下在那里没有语义），所以把按键原样转给状态机的话，上下键等于石沉
// 大海，而这一页的内容又必然超过一屏 —— 结果就是后半段内容谁也看不到。
static lv_obj_t *s_scroll;

// 提示条文字。上下键现在真的能翻页了，所以提示里必须把这件事说出来 ——
// 否则用户仍然不知道后半段内容怎么才能看到（这正是这一页原来的毛病）。
static void reveal_update_hint(void);

// 在当前内容量下，**往下还能滚多少像素**。注意这是个相对量，不是绝对位置 ——
// 拿它去夹「我想滚到第几像素」会让位置往回跳。位置一概交给 LVGL 自己维护。
// 必须在布局更新之后问：标签刚换了文字、高度还没重算，答案会是上一次的。
static int reveal_scroll_remaining(void)
{
    if (!s_scroll) return 0;
    lv_obj_update_layout(s_scroll);
    return (int)lv_obj_get_scroll_bottom(s_scroll);
}

// 翻一页。负的 dy 表示内容上移、露出下面的内容；LVGL 自己会把滚动位置夹在合法
// 范围内，所以到顶或到底之后再按只是不动，不会越界，也不需要我们先算边界。
static void reveal_scroll_by(int delta)
{
    if (!s_scroll) return;
    lv_obj_scroll_by(s_scroll, 0, -delta, LV_ANIM_ON);
    reveal_update_hint();
}

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

    if (s_hint) reveal_update_hint();
}

// 提示条一行只有 13 个字（16px 字库、可用宽 218px），所以两件事都要短：
// 「下面还有没有内容」+「确定键去哪」。第一件事原来根本没人说 —— 那正是这一页
// 的毛病：内容明明超出一屏，界面上却没有任何线索告诉用户还能往下看。
static void reveal_update_hint(void)
{
    if (!s_hint) return;

    const qpq_session_t *session = qpq_app_session();
    const bool last = session && session->position + 1 >= session->run_length;
    const bool more = reveal_scroll_remaining() > 0;

    char hint[48];
    snprintf(hint, sizeof(hint), "%s · %s", more ? "上下翻阅" : "已到底",
             last ? "确定 看成绩" : "确定 下一题");
    lv_label_set_text(s_hint, hint);
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
    s_scroll = NULL;

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
    s_scroll = scroll;

    // 不画滚动条。容器横跨整个卡面，滚动条会落在卡片的圆角上被切掉；更要紧的是
    // 正文的每行字数预算是按「可用宽 210px」算死的，为滚动条再缩 6px 会让 16px
    // 那档 13 字的一行（208px）放不下、被 LVGL 再折一次 —— 那正是这套预算要防的事。
    // 「下面还有内容」这件事交给提示条说，比一根压在圆角上的细条清楚。
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_OFF);

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
    // 页面每次进入都是重建的（导航层建新页再删旧页），所以滚动位置天然从顶部
    // 开始，不需要在这里保留或复位。这里只清指针，避免留下已释放对象的地址。
    s_scroll = NULL;
}

void qpq_page_reveal_key(qpq_key_t key)
{
    // 上/下键先由本页消费：状态机在判卷阶段对它们没有语义（会返回 ACT_NONE），
    // 而这一页的内容必然超过一屏 —— 交给状态机等于把上下键扔掉，后半段内容
    // 谁也别想看到。这里把它们接到滚动上，也就是用户本来以为会发生的事。
    if (key == QPQ_KEY_UP || key == QPQ_KEY_DOWN) {
        reveal_scroll_by(key == QPQ_KEY_DOWN ? REVEAL_SCROLL_STEP : -REVEAL_SCROLL_STEP);
        qpq_player_play_tone(QPQ_TONE_MOVE);
        return;
    }

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
