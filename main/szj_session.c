// main/szj_session.c —— 见 szj_session.h。
#include "szj_session.h"

#include <string.h>

static void session_clear(szj_session_t *session) {
    memset(session, 0, sizeof(*session));
    session->lesson = -1;
}

int szj_session_star_cap(void) {
    return SZJ_QUESTIONS_PER_LESSON;
}

bool szj_session_start_lesson(szj_session_t *session, int lesson) {
    if (!session) return false;

    const int count = szj_quiz_question_count(lesson);
    if (count <= 0 || count > SZJ_SESSION_MAX_QUESTIONS) {
        session_clear(session);
        return false;
    }

    szj_question_t built[SZJ_SESSION_MAX_QUESTIONS];
    for (int i = 0; i < count; i++) {
        if (!szj_quiz_build(lesson, i, &built[i])) {
            session_clear(session);
            return false;
        }
    }

    session_clear(session);
    session->kind = SZJ_SESSION_KIND_LESSON;
    session->lesson = lesson;
    memcpy(session->questions, built, sizeof(built[0]) * (size_t)count);
    session->count = count;
    return true;
}

bool szj_session_start_review(szj_session_t *session, const int *lines, int line_count) {
    if (!session) return false;
    if (!lines || line_count <= 0) {
        session_clear(session);
        return false;
    }
    if (line_count > SZJ_SESSION_MAX_QUESTIONS) line_count = SZJ_SESSION_MAX_QUESTIONS;

    session_clear(session);
    session->kind = SZJ_SESSION_KIND_REVIEW;

    // 单条错句生成失败不该拖垮整场复习,跳过它继续排后面的题。
    int built = 0;
    for (int i = 0; i < line_count; i++) {
        if (szj_quiz_build_review(lines[i], i, &session->questions[built])) built++;
    }
    if (built == 0) {
        session_clear(session);
        return false;
    }
    session->count = built;
    return true;
}

const szj_question_t *szj_session_current(const szj_session_t *session) {
    if (!session) return NULL;
    if (session->index < 0 || session->index >= session->count) return NULL;
    return &session->questions[session->index];
}

bool szj_session_current_answered(const szj_session_t *session) {
    if (!session) return false;
    if (session->index < 0 || session->index >= session->count) return false;
    return session->answered[session->index];
}

bool szj_session_done(const szj_session_t *session) {
    if (!session) return true;
    return session->index >= session->count;
}

bool szj_session_answer(szj_session_t *session, int slot) {
    if (!session) return false;
    if (slot < 0 || slot >= SZJ_QUIZ_OPTIONS) return false;

    const szj_question_t *question = szj_session_current(session);
    if (!question) return false;

    const bool is_correct = (slot == question->correct_slot);
    if (!session->answered[session->index]) {
        session->answered[session->index] = true;
        session->correct[session->index] = is_correct;
        if (is_correct) session->correct_count++;
    }
    return is_correct;
}

bool szj_session_next(szj_session_t *session) {
    if (!session) return false;
    if (session->index >= session->count) return false;
    session->index++;
    return session->index < session->count;
}

int szj_session_total(const szj_session_t *session) {
    return session ? session->count : 0;
}

int szj_session_index(const szj_session_t *session) {
    return session ? session->index : 0;
}

int szj_session_correct(const szj_session_t *session) {
    return session ? session->correct_count : 0;
}

bool szj_session_is_review(const szj_session_t *session) {
    return session && session->kind == SZJ_SESSION_KIND_REVIEW;
}
