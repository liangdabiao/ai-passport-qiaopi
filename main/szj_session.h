// main/szj_session.h —— 一次练习会话的状态机。
//
// 会话把两种玩法收敛到同一套流程:跟着课走的「接句闯关」,以及把答错过的句子
// 重新排一遍的「错题重练」。界面只负责画和按键,进度、判分、推进都在这里算,
// 因此本文件不依赖 ESP-IDF、LVGL 与 NVS,可在宿主上直接测试。
#pragma once

#include <stdbool.h>

#include "szj_quiz.h"

// 一课 3 题;错题重练一次最多这么多题,免得一次练习拖太长。
#define SZJ_SESSION_MAX_QUESTIONS 6

typedef enum {
    SZJ_SESSION_KIND_LESSON = 0,   // 跟着课走
    SZJ_SESSION_KIND_REVIEW,       // 错题重练
} szj_session_kind_t;

typedef struct {
    szj_session_kind_t kind;
    int lesson;                                   // 正式模式:课号(0 基);复习模式:-1
    szj_question_t questions[SZJ_SESSION_MAX_QUESTIONS];
    bool answered[SZJ_SESSION_MAX_QUESTIONS];     // 本题是否已提交过
    bool correct[SZJ_SESSION_MAX_QUESTIONS];      // 首次提交是否答对
    int count;                                    // 题目总数
    int index;                                    // 当前题号;等于 count 表示已结束
    int correct_count;                            // 首次即答对的题数
} szj_session_t;

// 以第 lesson 课建立会话。课号非法或题目生成失败返回 false,且不改动 session。
bool szj_session_start_lesson(szj_session_t *session, int lesson);

// 以一批「下句」下标建立错题复习会话(通常来自 szj_progress_wrong_list)。
// lines 为空或全部生成失败返回 false。
bool szj_session_start_review(szj_session_t *session, const int *lines, int line_count);

// 当前题目;会话已结束返回 NULL。
const szj_question_t *szj_session_current(const szj_session_t *session);

// 当前题是否已经提交过答案。
bool szj_session_current_answered(const szj_session_t *session);

// 会话是否已结束(含未成功建立的情况)。
bool szj_session_done(const szj_session_t *session);

// 提交当前题的作答,slot 为选项下标。返回本题是否答对。
// 同一题重复提交只记录第一次的结果,返回值仍反映本次选择是否正确。
bool szj_session_answer(szj_session_t *session, int slot);

// 推进到下一题;返回 false 表示会话已结束。
bool szj_session_next(szj_session_t *session);

int szj_session_total(const szj_session_t *session);
int szj_session_index(const szj_session_t *session);
int szj_session_correct(const szj_session_t *session);
bool szj_session_is_review(const szj_session_t *session);

// 正式模式一课最多能拿几颗星(= 题数)。
int szj_session_star_cap(void);
