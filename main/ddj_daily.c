// main/ddj_daily.c —— 日课的四层。
//
// 一屏只给一小口：原文一屏一句，点拨一条一屏，参究一问三选，最后把表态记下。
// 三个键的语义在第 4 层就只剩「结束」一个动作了，所以按键分发直接交给
// ddj_session（纯逻辑、有宿主测试），这一层只负责「按状态把屏画出来」。
//
// 一个刻意的取舍：这个应用不判分。参究的三个选项没有对错，只表示三种倾向
// （接了 / 还要想 / 没接上）。所以全文件没有一处「对/错」配色，声音也没有。
#include "ddj_daily.h"

#include <stddef.h>
#include <stdio.h>

#include "ddj_app.h"
#include "ddj_chapter.h"
#include "ddj_progress.h"
#include "ddj_sound.h"
#include "ddj_store.h"
#include "ddj_ui.h"

// 版式（相对内容容器，230x248）。竖向的账必须算清楚，否则「一屏放得下」只是
// 一句空话：
//   原文：小字 2..22 / 经文 24..248（224px）—— 最多 5 行 x 38px + 行距 6 = 214
//   点拨：正文 2..246（244px）—— 最多 7 行 x 29px + 行距 4 = 227
//         点拨不摆小字标题，就是为了空出这 20px；进度改在底栏报。
//   参究：小字 2..22 / 问题 22..90 / 三个选项 96..244
//   记下：面板 14..164 / 脚注 190
#define DAILY_CAPTION_Y 2
#define DAILY_READ_Y    24
#define DAILY_READ_H    (DDJ_BODY_H - DAILY_READ_Y)   // 224
#define DAILY_POINT_Y   2
#define DAILY_POINT_H   (DDJ_BODY_H - 4)              // 244
#define DAILY_QA_Y      2
#define DAILY_QUESTION_Y 22
#define DAILY_QUESTION_H 68
#define DAILY_OPTION_Y  96
#define DAILY_OPTION_GAP 8

#define DAILY_OPTION_COUNT 3   // 与 ddj_text.h 的 DDJ_PONDER_OPTION_COUNT 对齐

static lv_obj_t *s_body;
static lv_obj_t *s_hint;
static ddj_row_t s_options[DAILY_OPTION_COUNT];
static ddj_session_t s_session;
static bool s_sealed;   // 这一次日课已经计过一次数，不再重复累加

// ---------------------------------------------------------------- 存档 -----

// 把表态写进进度并落盘。
//
// 表态每次都写（用户可能退回去改主意），但「日课次数」一次日课只加一次 ——
// 否则退回去重选一次就会多算一节课。两个动作分开，语义才是对的。
static void daily_seal(void)
{
    ddj_progress_t *progress = ddj_store_progress();
    if (!progress) return;

    ddj_progress_set_slot(progress, s_session.chapter, ddj_session_slot(&s_session));
    ddj_progress_mark_read(progress, s_session.chapter);
    if (!s_sealed) {
        ddj_progress_end_session(progress);
        s_sealed = true;
    }
    (void)ddj_store_save();
}

// ---------------------------------------------------------------- 各层 -----

static void build_read(const ddj_chapter_t *chapter)
{
    char label[DDJ_CHAPTER_LABEL_CAPACITY];
    if (!ddj_chapter_label(chapter->number, label, sizeof(label))) label[0] = '\0';

    // 两个 %s 都取自封闭集合：卷名恒为两个字（6 字节），章号文案最多五个字
    // （「第八十一章」15 字节）。最坏 6 + 4 + 15 + 1 = 27，48 留足余量。
    char caption[48];
    snprintf(caption, sizeof(caption), "%s · %s", ddj_volume_name(chapter->volume), label);
    ddj_note_create(s_body, DDJ_BODY_X, DAILY_CAPTION_Y, DDJ_BODY_W - 52, caption,
                    DDJ_C_MUTED);

    // 按 int 的最宽十进制形态开：两个 %d 各 11 字节（含符号位）+ 斜杠 + 结尾符
    // = 24。真实值只有「1/6」这么大，但缓冲区不该按真实值开 —— 见
    // docs/reference/liangdabiao/font-metrics-driven-layout.md。
    char counter[24];
    snprintf(counter, sizeof(counter), "%d/%d", s_session.passage + 1,
             (int)chapter->passage_count);
    lv_obj_t *right = ddj_note_create(s_body, DDJ_PAGE_W - DDJ_BODY_X - 44,
                                      DAILY_CAPTION_Y, 44, counter, DDJ_C_MUTED);
    if (right) lv_obj_set_style_text_align(right, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *passage = ddj_passage_create(s_body, DDJ_BODY_X, DAILY_READ_Y, DDJ_BODY_W,
                                           DAILY_READ_H);
    ddj_text_set(passage, ddj_chapter_passage(chapter, s_session.passage),
                 DDJ_CHARS_PASSAGE);
}

static void build_point(const ddj_chapter_t *chapter)
{
    lv_obj_t *point = ddj_paragraph_create(s_body, DDJ_BODY_X, DAILY_POINT_Y, DDJ_BODY_W,
                                           DAILY_POINT_H);
    ddj_text_set(point, ddj_chapter_point(chapter, s_session.point), DDJ_CHARS_PARA);
}

static void build_ponder(const ddj_chapter_t *chapter)
{
    ddj_note_create(s_body, DDJ_BODY_X, DAILY_QA_Y, DDJ_BODY_W, "参究", DDJ_C_CINNABAR);

    lv_obj_t *question = ddj_label_create(s_body, "", &ddj_font_16, DDJ_C_INK);
    lv_obj_set_pos(question, DDJ_BODY_X, DAILY_QUESTION_Y);
    lv_obj_set_width(question, DDJ_BODY_W);
    lv_obj_set_height(question, DAILY_QUESTION_H);
    lv_label_set_long_mode(question, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(question, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_line_space(question, 4, 0);
    ddj_text_set(question, chapter->question, DDJ_CHARS_NOTE);

    for (int i = 0; i < DAILY_OPTION_COUNT; i++) {
        const int y = DAILY_OPTION_Y + i * (DDJ_ROW_H + DAILY_OPTION_GAP);
        // note_width 传 0：选项要用满整行（7 个字 x 24px = 168px）。
        s_options[i] = ddj_row_create(s_body, DDJ_BODY_X, y, DDJ_BODY_W, DDJ_ROW_H, 0);
        ddj_row_update(&s_options[i], ddj_chapter_option(chapter, i), "",
                       i == s_session.cursor ? DDJ_STATE_SELECTED : DDJ_STATE_NORMAL);
    }
}

static void build_done(const ddj_chapter_t *chapter)
{
    lv_obj_t *panel = ddj_panel_create(s_body, DDJ_BODY_X, 14, DDJ_BODY_W, 150,
                                       DDJ_C_CINNABAR_S, DDJ_C_CINNABAR, 3);

    lv_obj_t *head = ddj_label_create(panel, "参究已记下", &ddj_font_24, DDJ_C_CINNABAR_D);
    if (head) lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 16);

    // 把用户自己选的那一句再给他看一遍 —— 这是这一层唯一要传达的东西。
    const char *chosen = ddj_chapter_option(chapter, s_session.cursor);
    lv_obj_t *line = ddj_label_create(panel, chosen ? chosen : "", &ddj_font_24, DDJ_C_INK);
    if (line) lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 58);

    // 上一回的表态：日课机「反复参」的那一半，靠的就是这句话。
    if (s_session.previous != DDJ_SLOT_NONE) {
        // %s 取自 ddj_slot_name 那个封闭集合（「还要想」最多 3 字 = 9 字节）：
        // 最坏 9 + 9 + 1 = 19，32 留足余量。
        char previous[32];
        snprintf(previous, sizeof(previous), "上次：%s", ddj_slot_name((int)s_session.previous));
        lv_obj_t *note = ddj_label_create(panel, previous, &ddj_font_16, DDJ_C_MUTED);
        if (note) lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 108);
    }

    const ddj_progress_t *progress = ddj_store_progress();
    if (!progress) return;

    char foot[64];
    snprintf(foot, sizeof(foot), "日课 %d 次 · 已读 %d/%d 章",
             ddj_progress_sessions(progress), ddj_progress_read_count(progress),
             ddj_chapter_count());
    lv_obj_t *label = ddj_label_create(s_body, foot, &ddj_font_16, DDJ_C_INK_SOFT);
    if (!label) return;
    lv_obj_set_width(label, DDJ_PAGE_W);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, 190);
}

// 统一的「重画当前层」。任何导致状态变化的按键之后都走这里，屏就不会和
// 状态脱节 —— 这类「状态变了屏没变」的 bug 靠流程避免，不靠自觉。
static void daily_build(void)
{
    if (!s_body) return;
    lv_obj_clean(s_body);
    for (int i = 0; i < DAILY_OPTION_COUNT; i++) s_options[i] = (ddj_row_t){0};

    const ddj_chapter_t *chapter = ddj_chapter_at(s_session.chapter);
    if (!chapter) return;

    const char *hint = "";
    switch (s_session.stage) {
        case DDJ_STAGE_READ:
            build_read(chapter);
            hint = "确定 下一句 · 长按 返回";
            break;
        case DDJ_STAGE_POINT: {
            build_point(chapter);
            // 点拨层不摆小字标题（要让出 20px 给正文），进度就报在底栏。
            // 字面量 28 字节 + 两个最宽 %d（各 11）+ 结尾符 = 51，取 56。
            static char point_hint[56];
            snprintf(point_hint, sizeof(point_hint), "点拨 %d/%d · 确定 下一条",
                     s_session.point + 1, (int)chapter->point_count);
            hint = point_hint;
            break;
        }
        case DDJ_STAGE_PONDER:
            build_ponder(chapter);
            hint = "上/下 选择 · 确定 记下";
            break;
        default:
            build_done(chapter);
            hint = "确定 回到首页";
            break;
    }

    if (s_hint) lv_label_set_text(s_hint, hint);
}

// ---------------------------------------------------------------------------

lv_obj_t *ddj_daily_enter(int chapter)
{
    const ddj_chapter_t *entry = ddj_chapter_at(chapter);
    if (!entry) return NULL;

    s_body = NULL;
    s_hint = NULL;
    s_sealed = false;
    for (int i = 0; i < DAILY_OPTION_COUNT; i++) s_options[i] = (ddj_row_t){0};

    const ddj_progress_t *progress = ddj_store_progress();
    const ddj_slot_t previous =
        progress ? ddj_progress_slot(progress, chapter) : DDJ_SLOT_NONE;
    ddj_session_begin(&s_session, chapter, previous);

    char heading[DDJ_CHAPTER_HEADING_CAPACITY];
    if (!ddj_chapter_heading(entry, heading, sizeof(heading))) {
        if (!ddj_chapter_label(entry->number, heading, sizeof(heading))) heading[0] = '\0';
    }

    lv_obj_t *card = NULL;
    lv_obj_t *scr = ddj_page_create(&card);
    if (!card) return scr;

    ddj_topbar_create(card, heading);
    s_hint = ddj_hint_create(card, "");
    s_body = ddj_body_create(card);
    daily_build();
    return scr;
}

void ddj_daily_leave(void)
{
    s_body = NULL;
    s_hint = NULL;
    for (int i = 0; i < DAILY_OPTION_COUNT; i++) s_options[i] = (ddj_row_t){0};
}

void ddj_daily_key(ddj_key_t key)
{
    const ddj_chapter_t *chapter = ddj_chapter_at(s_session.chapter);
    if (!chapter) {
        ddj_app_goto_home();
        return;
    }

    const ddj_stage_t stage_before = s_session.stage;
    const int passage_before = s_session.passage;
    const int point_before = s_session.point;
    const int cursor_before = s_session.cursor;

    const ddj_act_t act = ddj_session_key(&s_session, key, (int)chapter->passage_count,
                                          (int)chapter->point_count, DAILY_OPTION_COUNT);

    if (act == DDJ_ACT_CANCEL) {
        // 第一屏就退出：这一章没读完，什么都不记。
        ddj_sound_play(DDJ_SOUND_BACK);
        ddj_app_goto_home();
        return;
    }
    if (act == DDJ_ACT_FINISHED) {
        // 表态在第 4 层进来时就已经落盘了，这里只负责回首页。
        ddj_sound_play(DDJ_SOUND_BACK);
        ddj_app_goto_home();
        return;
    }

    // 先落盘再画屏：这样「已记下」这三个字出现时，它已经是真的了。
    if (s_session.stage == DDJ_STAGE_DONE && stage_before != DDJ_STAGE_DONE) {
        daily_seal();
        ddj_sound_play(DDJ_SOUND_SEAL);
    } else if (s_session.stage != stage_before) {
        ddj_sound_play(DDJ_SOUND_ENTER);
    } else if (s_session.passage != passage_before || s_session.point != point_before ||
               s_session.cursor != cursor_before) {
        ddj_sound_play(DDJ_SOUND_MOVE);
    }

    daily_build();
}
