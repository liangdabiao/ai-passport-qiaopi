// main/qpq_session.h —— 一局的答题状态机。
//
// 规则与网页版一致（见 tools/qiaopi/content.py 里记的常量）：每局抽
// QPQ_RUN_LENGTH 题，答对得 QPQ_SCORE_CORRECT，连对达 QPQ_STREAK_BONUS_AT 次及以上
// 追加 QPQ_SCORE_STREAK_BONUS，答错扣 QPQ_SCORE_WRONG_PENALTY 且分数不为负。
//
// 本模块是纯逻辑：不碰 LVGL、不碰 ESP-IDF、不读时钟、不自己抽随机数种子。
// 随机数用自带的 xorshift32，种子由调用方给 —— 这正是「抽题结果可复现」成为
// 可测事实的原因，否则「每局 20 题不重复」这类断言只能靠运气。
//
// 一处有意与网页版不同：优先抽「还没见过」的题（见 qpq_session_set_seen）。
// 网页版是均匀随机，因为浏览器没有跨局的记忆；设备有 NVS，能让读者先见全 91 题。
// 这条规则是可测的：见 tests/test_qpq_session.c。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "qpq_text.h"

// 已见题位图的字节数：91 题 → 12 字节。
#define QPQ_SEEN_BYTES ((QPQ_QUESTION_COUNT + 7) / 8)

typedef enum {
    QPQ_STAGE_TITLE = 0,   // 标题页
    QPQ_STAGE_ASK,         // 答题页：句子 + 四个候选
    QPQ_STAGE_REVEAL,      // 判卷页：对错 + 解析 + 完整原文
    QPQ_STAGE_SUMMARY,     // 结算页
    QPQ_STAGE_COUNT,
} qpq_stage_t;

typedef enum {
    QPQ_KEY_UP = 0,
    QPQ_KEY_DOWN,
    QPQ_KEY_OK,
    QPQ_KEY_BACK,
    QPQ_KEY_COUNT,
} qpq_key_t;

// 一次按键产生的副作用。界面层据此决定要刷新什么、要不要放音。
typedef enum {
    QPQ_ACT_NONE = 0,      // 什么都没发生（例如标题页按了上／下）
    QPQ_ACT_STARTED,       // 开始了一局，进入答题页
    QPQ_ACT_MOVED,         // 选中项变化，只需重画高亮
    QPQ_ACT_ANSWERED,      // 已判卷，应当朗读本题的方言配音
    QPQ_ACT_ADVANCED,      // 进入下一题（仍是答题页）
    QPQ_ACT_FINISHED,      // 本局结束，进入结算页
    QPQ_ACT_LEFT,          // 放弃本局，回到标题页
} qpq_action_t;

typedef struct {
    const qpq_question_t *question;
    bool correct;
} qpq_history_entry_t;

typedef struct {
    qpq_stage_t stage;

    uint16_t order[QPQ_RUN_LENGTH];    // 本局抽到的题目下标
    uint16_t run_length;               // 题库不足时可能小于 QPQ_RUN_LENGTH
    uint16_t position;                 // 当前第几题，0 基
    uint8_t cursor;                    // 当前选中的候选 0..3
    uint8_t chosen;                    // 玩家实际选的候选
    bool answered;

    int score;
    uint16_t correct_count;
    uint16_t streak;
    uint16_t max_streak;

    qpq_history_entry_t history[QPQ_RUN_LENGTH];
    uint16_t history_count;

    uint32_t rng;                      // xorshift32 状态
    const uint8_t *seen;               // 已见题位图，可为 NULL
} qpq_session_t;

// 复位到标题页。种子为 0 时用一个固定常量，避免 xorshift32 卡在零。
void qpq_session_init(qpq_session_t *session, uint32_t seed);

// 登记已见题位图（QPQ_SEEN_BYTES 字节，可为 NULL）。指针由调用方持有。
void qpq_session_set_seen(qpq_session_t *session, const uint8_t *seen_bitmap);

// 开始一局：抽题、清零计分、进入答题页。
qpq_action_t qpq_session_start(qpq_session_t *session);

// 处理一次按键，返回应当执行的副作用。
qpq_action_t qpq_session_key(qpq_session_t *session, qpq_key_t key);

// 当前题目；不在答题／判卷阶段时返回 NULL。
const qpq_question_t *qpq_session_question(const qpq_session_t *session);

// 当前题的方言配音片段号。音频片段号与题目下标一一对应，所以这等于题目下标；
// 不可用时返回 UINT16_MAX。
uint16_t qpq_session_narration_clip(const qpq_session_t *session);

// 本题是否答对。未作答时恒为 false。
bool qpq_session_is_correct(const qpq_session_t *session);

// 正确率（0..100，四舍五入，与网页版的 Math.round 一致）。
int qpq_session_percent(const qpq_session_t *session);

// 结算评级；不在结算阶段时也返回按当前正确率算出的档位，方便界面直接显示。
const qpq_rank_t *qpq_session_rank(const qpq_session_t *session);
