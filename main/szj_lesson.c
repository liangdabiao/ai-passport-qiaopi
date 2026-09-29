// main/szj_lesson.c —— 接句闯关。
//
// 题面:显示「上句」三个大字,下面三个选项各是一个「下句」,选对即过关。
// 答完立刻把对错用颜色和声音同时告诉孩子(颜色给看得懂字的,声音给还没认全的),
// 停 0.9~1.6 秒自动翻下一题 —— 这样不需要「下一题」按钮,三个键就够用。
//
// 错题重练复用同一套界面,只是题库来自错题位图,一次最多 6 题,答对即销账。
#include "szj_lesson.h"

#include <stddef.h>
#include <stdio.h>

#include "szj_progress.h"
#include "szj_quiz.h"
#include "szj_session.h"
#include "szj_sound.h"
#include "szj_store.h"
#include "szj_text.h"
#include "szj_text_util.h"
#include "szj_ui.h"

// 版式(相对内容容器 s_body,230x248)
//
// 竖向账要算清楚:题号 6..26 / 题面 32..92 / 三个选项 96..242,底 6px 余量。
// 选项行高 46 同样是被字库逼出来的:32px 字的 line_height 是 38,行有 3px 边框,
// 内容区必须 >= 38,也就是行高至少 44。40 会把选项上下各切掉 2px。
#define LESSON_CAPTION_Y   6
#define LESSON_PROMPT_Y    32
#define LESSON_PROMPT_H    60
#define LESSON_OPTION_Y    96
#define LESSON_OPTION_H    46
#define LESSON_OPTION_GAP  4

#define LESSON_DOT_SIZE    14
#define LESSON_DOT_GAP     6

// 答对后停顿短一点,答错要留时间把正确答案看进眼里。
#define LESSON_DELAY_OK_MS     900
#define LESSON_DELAY_WRONG_MS  1600

static lv_obj_t *s_body;
static lv_obj_t *s_hint;
static lv_obj_t *s_dots[SZJ_SESSION_MAX_QUESTIONS];
static szj_row_t s_options[SZJ_QUIZ_OPTIONS];
static szj_session_t s_session;
static lv_timer_t *s_timer;
static int s_sel;
static bool s_locked;    // 本题已作答,等自动翻页
static bool s_result;    // 正在显示成绩页
static bool s_dirty;     // 进度对象被改过,需要落盘

// ---------------------------------------------------------------------------

static void body_clear(void)
{
    if (s_body) lv_obj_clean(s_body);
    for (int i = 0; i < SZJ_SESSION_MAX_QUESTIONS; i++) s_dots[i] = NULL;
    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) s_options[i] = (szj_row_t){0};
}

static void options_refresh(void)
{
    const szj_question_t *question = szj_session_current(&s_session);
    if (!question) return;
    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) {
        szj_option_update(&s_options[i], szj_line(question->options[i]),
                          i == s_sel ? SZJ_STATE_SELECTED : SZJ_STATE_NORMAL);
    }
}

static void question_build(void)
{
    body_clear();

    const szj_question_t *question = szj_session_current(&s_session);
    if (!question || !s_body) return;

    const int index = szj_session_index(&s_session);
    const int total = szj_session_total(&s_session);

    // 缓冲区按 int 的最坏十进制宽度取,别按实际题数取 —— 实际最多 6 题,
    // 但 GCC 的 -Wformat-truncation 只认类型范围,按实际值开数组会报截断错误。
    char caption[48];
    snprintf(caption, sizeof(caption), "第 %d/%d 题", index + 1, total);
    lv_obj_t *label = szj_label_create(s_body, caption, &szj_font_16, SZJ_C_RED);
    lv_obj_set_pos(label, SZJ_BODY_X, LESSON_CAPTION_Y);

    // 进度点从右往左排,和题号分处两端,中间留白给以后加东西。
    const int dots_w = total * LESSON_DOT_SIZE + (total - 1) * LESSON_DOT_GAP;
    int x = SZJ_PAGE_W - SZJ_BODY_X - dots_w;
    for (int i = 0; i < total; i++) {
        s_dots[i] = szj_dot_create(s_body, x, LESSON_CAPTION_Y, LESSON_DOT_SIZE);
        szj_dot_set(s_dots[i], i == index ? SZJ_DOT_CURRENT : SZJ_DOT_PENDING);
        x += LESSON_DOT_SIZE + LESSON_DOT_GAP;
    }

    lv_obj_t *panel = szj_panel_create(s_body, SZJ_BODY_X, LESSON_PROMPT_Y, SZJ_BODY_W,
                                       LESSON_PROMPT_H, SZJ_C_PAPER_ALT, SZJ_C_LINE, 3);
    lv_obj_t *prompt = szj_label_create(panel, szj_line(question->prompt_line),
                                        &szj_font_32, SZJ_C_INK);
    if (prompt) lv_obj_center(prompt);

    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) {
        const int y = LESSON_OPTION_Y + i * (LESSON_OPTION_H + LESSON_OPTION_GAP);
        s_options[i] = szj_option_create(s_body, SZJ_BODY_X, y, SZJ_BODY_W,
                                         LESSON_OPTION_H);
    }
    options_refresh();

    if (s_hint) lv_label_set_text(s_hint, "上/下 选择 · 确定 作答");
}

static void result_build(void)
{
    body_clear();
    if (!s_body) return;

    const bool review = szj_session_is_review(&s_session);
    const int correct = szj_session_correct(&s_session);
    const int total = szj_session_total(&s_session);
    const szj_progress_t *progress = szj_store_progress();

    lv_obj_t *panel = szj_panel_create(s_body, SZJ_BODY_X, 22, SZJ_BODY_W, 140,
                                       SZJ_C_GOLD_SOFT, SZJ_C_GOLD, 3);

    lv_obj_t *head = szj_label_create(panel, review ? "复习完成！" : "本课完成！",
                                      &szj_font_24, SZJ_C_RED_DARK);
    if (head) lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 10);

    if (review) {
        char score[24];
        snprintf(score, sizeof(score), "答对 %d/%d 题", correct, total);
        lv_obj_t *line = szj_label_create(panel, score, &szj_font_24, SZJ_C_INK);
        if (line) lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 56);
    } else {
        char stars[SZJ_STARS_TEXT_CAPACITY];
        szj_stars_text(correct, total, stars, sizeof(stars));
        lv_obj_t *line = szj_label_create(panel, stars, &szj_font_32, SZJ_C_GOLD);
        if (line) lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 52);
    }

    char foot[32];
    if (review && progress) {
        snprintf(foot, sizeof(foot), "错题本还剩 %d 句", szj_progress_wrong_count(progress));
    } else if (progress) {
        snprintf(foot, sizeof(foot), "已过关 %d/%d 课",
                 szj_progress_completed_lessons(progress), SZJ_LESSON_COUNT);
    } else {
        foot[0] = '\0';
    }
    lv_obj_t *foot_label = szj_label_create(panel, foot, &szj_font_16, SZJ_C_INK_SOFT);
    if (foot_label) lv_obj_align(foot_label, LV_ALIGN_TOP_MID, 0, 106);

    const char *note = (correct == total) ? "全部答对，真棒！"
                     : (review ? "答对的句子已从错题本移出"
                               : "答错的句子已记入错题本");
    lv_obj_t *note_label = szj_label_create(s_body, note, &szj_font_16, SZJ_C_INK_SOFT);
    if (note_label) lv_obj_align(note_label, LV_ALIGN_TOP_MID, 0, 178);

    if (s_hint) lv_label_set_text(s_hint, "确定 返回首页");
}

static void session_finish(void)
{
    const bool review = szj_session_is_review(&s_session);

    if (!review) {
        szj_progress_t *progress = szj_store_progress();
        if (progress) {
            szj_progress_record_lesson(progress, s_session.lesson,
                                       szj_session_correct(&s_session));
            s_dirty = true;
        }
    }
    if (s_dirty) {
        szj_store_save();
        s_dirty = false;
    }

    s_result = true;
    szj_sound_play(SZJ_SOUND_LESSON_DONE);
    result_build();
}

static void advance_timer(lv_timer_t *timer)
{
    (void)timer;
    s_timer = NULL;       // 一次性定时器由 LVGL 回收
    s_locked = false;

    if (szj_session_next(&s_session)) {
        s_sel = 0;
        question_build();
    } else {
        session_finish();
    }
}

static void answer_submit(void)
{
    if (s_locked || s_result) return;

    const szj_question_t *question = szj_session_current(&s_session);
    if (!question) return;

    s_locked = true;
    const bool ok = szj_session_answer(&s_session, s_sel);

    szj_progress_t *progress = szj_store_progress();
    if (progress) {
        if (!ok) {
            szj_progress_mark_wrong(progress, question->answer_line);
            s_dirty = true;
        } else if (szj_session_is_review(&s_session)) {
            // 复习时答对就把这条错账销掉。
            szj_progress_clear_wrong(progress, question->answer_line);
            s_dirty = true;
        }
    }

    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) {
        szj_state_t state = SZJ_STATE_NORMAL;
        if (i == s_sel) {
            state = ok ? SZJ_STATE_CORRECT : SZJ_STATE_WRONG;
        } else if (!ok && i == question->correct_slot) {
            state = SZJ_STATE_CORRECT;   // 答错时把正确的一项一起点亮
        }
        szj_option_update(&s_options[i], szj_line(question->options[i]), state);
    }

    const int index = szj_session_index(&s_session);
    if (index < SZJ_SESSION_MAX_QUESTIONS && s_dots[index]) {
        szj_dot_set(s_dots[index], ok ? SZJ_DOT_CORRECT : SZJ_DOT_WRONG);
    }

    szj_sound_play(ok ? SZJ_SOUND_CORRECT : SZJ_SOUND_WRONG);

    s_timer = lv_timer_create(advance_timer, ok ? LESSON_DELAY_OK_MS
                                               : LESSON_DELAY_WRONG_MS, NULL);
    if (s_timer) lv_timer_set_repeat_count(s_timer, 1);
}

// ---------------------------------------------------------------------------

static lv_obj_t *lesson_page_create(const char *title)
{
    s_body = NULL;
    s_hint = NULL;
    s_sel = 0;
    s_locked = false;
    s_result = false;
    s_timer = NULL;
    for (int i = 0; i < SZJ_SESSION_MAX_QUESTIONS; i++) s_dots[i] = NULL;
    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) s_options[i] = (szj_row_t){0};

    lv_obj_t *card = NULL;
    lv_obj_t *scr = szj_page_create(&card);
    if (!card) return scr;

    szj_topbar_create(card, title);
    s_hint = szj_hint_create(card, "");
    s_body = szj_body_create(card);
    question_build();
    return scr;
}

lv_obj_t *szj_lesson_enter(int lesson)
{
    if (!szj_session_start_lesson(&s_session, lesson)) return NULL;

    char title[24];
    snprintf(title, sizeof(title), "第 %d 课", lesson + 1);
    return lesson_page_create(title);
}

lv_obj_t *szj_review_enter(void)
{
    szj_progress_t *progress = szj_store_progress();
    if (!progress) return NULL;

    int lines[SZJ_SESSION_MAX_QUESTIONS];
    const int count = szj_progress_wrong_list(progress, lines, SZJ_SESSION_MAX_QUESTIONS);
    if (!szj_session_start_review(&s_session, lines, count)) return NULL;

    return lesson_page_create("错题重练");
}

void szj_lesson_leave(void)
{
    // 定时器回调会碰界面对象,必须先停掉再让调用方删屏。
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_dirty) {
        szj_store_save();
        s_dirty = false;
    }

    s_body = NULL;
    s_hint = NULL;
    s_locked = false;
    s_result = false;
    for (int i = 0; i < SZJ_SESSION_MAX_QUESTIONS; i++) s_dots[i] = NULL;
    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) s_options[i] = (szj_row_t){0};
}

void szj_lesson_key(szj_key_t key)
{
    if (s_result) {
        if (key == SZJ_KEY_OK || key == SZJ_KEY_BACK) {
            szj_sound_play(SZJ_SOUND_BACK);
            szj_app_goto_home();
        }
        return;
    }
    if (s_locked) return;   // 已作答,等自动翻页,这期间按键无效

    switch (key) {
        case SZJ_KEY_UP:
            if (s_sel == 0) return;
            s_sel--;
            szj_sound_play(SZJ_SOUND_MOVE);
            options_refresh();
            break;
        case SZJ_KEY_DOWN:
            if (s_sel + 1 >= SZJ_QUIZ_OPTIONS) return;
            s_sel++;
            szj_sound_play(SZJ_SOUND_MOVE);
            options_refresh();
            break;
        case SZJ_KEY_OK:
            answer_submit();
            break;
        case SZJ_KEY_BACK:
            szj_sound_play(SZJ_SOUND_BACK);
            szj_app_goto_home();
            break;
        default:
            break;
    }
}
