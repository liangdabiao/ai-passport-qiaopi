// main/szj_quiz.h —— 「接句闯关」的出题与判分逻辑。
//
// 全部为纯计算:不依赖 ESP-IDF、LVGL、NVS,可在宿主上直接测试。
// 同一 (课, 题) 永远生成同一道题,便于测试与「错题重练」复现。
#pragma once

#include <stdbool.h>

#define SZJ_QUIZ_OPTIONS 3
#define SZJ_QUESTIONS_PER_LESSON 3

typedef struct {
    int lesson;                        // 课号(0 基);复习模式为 -1
    int question;                      // 课内题号(0 基)
    int prompt_line;                   // 题干「上句」的全局下标
    int answer_line;                   // 正确「下句」的全局下标
    int options[SZJ_QUIZ_OPTIONS];     // 三个选项的全局下标,顺序固定
    int correct_slot;                  // 正确项在 options 中的下标
} szj_question_t;

// 第 lesson 课的题目数量;课号非法返回 0。
int szj_quiz_question_count(int lesson);

// 生成第 lesson 课第 question 题。越界返回 false 且不修改 out。
bool szj_quiz_build(int lesson, int question, szj_question_t *out);

// 复习模式:针对一个答错过的「下句」重新出题(题干取它的上一句)。
bool szj_quiz_build_review(int answer_line, int question, szj_question_t *out);

// 一项答对得一颗星;星星数 = 首次作答即正确的题数(0..SZJ_QUESTIONS_PER_LESSON)。
int szj_quiz_star_cap(void);
