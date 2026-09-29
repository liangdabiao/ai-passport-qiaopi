// 由 tools/qiaopi/gen_content.py 从 tools/qiaopi/bank.txt 生成。
// 请勿手改；改题库源文件后重跑生成器（validate.sh 会用 --check 拦住过期的表）。

#pragma once

#include <stdint.h>

/* 题库规模。每局抽 QPQ_RUN_LENGTH 题；题库不足时由逻辑层夹取。 */
#define QPQ_QUESTION_COUNT 91
#define QPQ_OPTION_COUNT 4
#define QPQ_ANSWER_SLOT_COUNT 4
#define QPQ_CATEGORY_COUNT 6
#define QPQ_RUN_LENGTH 20
#define QPQ_RANK_COUNT 5

/* 填空标记：题目句里用它切出设备上的空格位。 */
#define QPQ_BLANK "____"

/* 计分规则，与网页版一致：答对得 QPQ_SCORE_CORRECT，连对达
 * QPQ_STREAK_BONUS_AT 次及以上追加 QPQ_SCORE_STREAK_BONUS，答错扣
 * QPQ_SCORE_WRONG_PENALTY，且分数不会低于零。 */
#define QPQ_SCORE_CORRECT 10
#define QPQ_SCORE_STREAK_BONUS 2
#define QPQ_STREAK_BONUS_AT 2
#define QPQ_SCORE_WRONG_PENALTY 3

/* 设备版式预算，来源是 tools/qiaopi/content.py 顶部那张算术推导。
 * 宿主测试拿真实题库断言折行结果不超这些值。 */
#define QPQ_CATEGORY_MAX_CHARS 6
#define QPQ_SENTENCE_MAX_CHARS 22
#define QPQ_SENTENCE_FILLED_MAX_CHARS 21
#define QPQ_OPTION_MAX_CHARS 6
#define QPQ_EXPLAIN_MAX_CHARS 60
#define QPQ_FULL_MAX_CHARS 48
#define QPQ_SOURCE_MAX_CHARS 26

typedef struct {
    const char *category;                    /* 分类，答题页顶栏 */
    const char *sentence;                    /* 题目句，含 QPQ_BLANK */
    const char *options[QPQ_OPTION_COUNT];   /* 四个候选，顺序固定 */
    const char *explain;                     /* 解析，含典故出处 */
    const char *source;                      /* 侨批来源，寄信人与年份 */
    const char *full;                        /* 完整原文 */
    uint8_t answer;                          /* 正确候选下标 0..3 */
} qpq_question_t;

/* 结算评级：percent 是正确率下限，从高到低排列，取第一个满足的档。 */
typedef struct {
    uint8_t percent;
    const char *rank;
    const char *subtitle;
    const char *description;
} qpq_rank_t;

extern const qpq_question_t qpq_questions[QPQ_QUESTION_COUNT];
extern const char *const qpq_answer_slots[QPQ_ANSWER_SLOT_COUNT];
extern const char *const qpq_categories[QPQ_CATEGORY_COUNT];
extern const qpq_rank_t qpq_ranks[QPQ_RANK_COUNT];
