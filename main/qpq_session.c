// main/qpq_session.c —— 见 qpq_session.h。
// 不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#include "qpq_session.h"

#include <stddef.h>

#include "qpq_audio.h"
#include "qpq_content.h"

// xorshift32：状态小、无分配、结果只取决于种子。用它而不是 rand()，是因为
// 「抽题可复现」必须成立，否则测试只能碰运气。
static uint32_t next_random(qpq_session_t *session)
{
    uint32_t state = session->rng;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    session->rng = state;
    return state;
}

// 返回 [0, bound) 内的一个数。bound 为 0 时返回 0。
static uint16_t random_below(qpq_session_t *session, uint16_t bound)
{
    if (bound == 0) return 0;
    return (uint16_t)(next_random(session) % bound);
}

static void shuffle(qpq_session_t *session, uint16_t *values, uint16_t count)
{
    for (uint16_t i = count; i > 1; i--) {
        const uint16_t j = random_below(session, i);
        const uint16_t swap = values[i - 1];
        values[i - 1] = values[j];
        values[j] = swap;
    }
}

static bool is_seen(const qpq_session_t *session, uint16_t index)
{
    if (!session->seen) return false;
    return (session->seen[index >> 3] & (uint8_t)(1u << (index & 7))) != 0;
}

void qpq_session_init(qpq_session_t *session, uint32_t seed)
{
    if (!session) return;
    for (size_t i = 0; i < sizeof(*session); i++) ((uint8_t *)session)[i] = 0;
    session->rng = seed ? seed : 0x9E3779B9u;
    session->stage = QPQ_STAGE_TITLE;
}

void qpq_session_set_seen(qpq_session_t *session, const uint8_t *seen_bitmap)
{
    if (!session) return;
    session->seen = seen_bitmap;
}

qpq_action_t qpq_session_start(qpq_session_t *session)
{
    if (!session) return QPQ_ACT_NONE;

    const uint16_t total = qpq_question_count();
    if (total == 0) {
        session->stage = QPQ_STAGE_TITLE;
        return QPQ_ACT_NONE;
    }

    // 先分池再各自打散：没见过的题优先，不够时用见过的补齐。
    uint16_t unseen[QPQ_QUESTION_COUNT];
    uint16_t seen[QPQ_QUESTION_COUNT];
    uint16_t unseen_count = 0;
    uint16_t seen_count = 0;
    for (uint16_t index = 0; index < total; index++) {
        if (is_seen(session, index)) {
            seen[seen_count++] = index;
        } else {
            unseen[unseen_count++] = index;
        }
    }
    shuffle(session, unseen, unseen_count);
    shuffle(session, seen, seen_count);

    uint16_t target = total < (uint16_t)QPQ_RUN_LENGTH ? total : (uint16_t)QPQ_RUN_LENGTH;
    uint16_t filled = 0;
    for (uint16_t i = 0; i < unseen_count && filled < target; i++) {
        session->order[filled++] = unseen[i];
    }
    for (uint16_t i = 0; i < seen_count && filled < target; i++) {
        session->order[filled++] = seen[i];
    }

    session->run_length = filled;
    session->position = 0;
    session->cursor = 0;
    session->chosen = 0;
    session->answered = false;
    session->score = 0;
    session->correct_count = 0;
    session->streak = 0;
    session->max_streak = 0;
    session->history_count = 0;
    session->stage = QPQ_STAGE_ASK;
    return QPQ_ACT_STARTED;
}

qpq_action_t qpq_session_key(qpq_session_t *session, qpq_key_t key)
{
    if (!session) return QPQ_ACT_NONE;

    switch (session->stage) {
        case QPQ_STAGE_TITLE:
            if (key == QPQ_KEY_OK) return qpq_session_start(session);
            return QPQ_ACT_NONE;

        case QPQ_STAGE_ASK: {
            if (key == QPQ_KEY_UP || key == QPQ_KEY_DOWN) {
                const uint8_t count = (uint8_t)QPQ_OPTION_COUNT;
                if (count == 0) return QPQ_ACT_NONE;
                if (key == QPQ_KEY_UP) {
                    session->cursor = (uint8_t)((session->cursor + count - 1) % count);
                } else {
                    session->cursor = (uint8_t)((session->cursor + 1) % count);
                }
                return QPQ_ACT_MOVED;
            }
            if (key == QPQ_KEY_BACK) {
                session->stage = QPQ_STAGE_TITLE;
                return QPQ_ACT_LEFT;
            }
            if (key != QPQ_KEY_OK) return QPQ_ACT_NONE;

            const qpq_question_t *question = qpq_session_question(session);
            if (!question) return QPQ_ACT_NONE;

            session->chosen = session->cursor;
            session->answered = true;

            const bool correct = session->chosen == question->answer;
            if (correct) {
                // 追加奖励看的是**自增之前**的连对数，与网页版一致：第三次连对才拿到
                // 加成（此时 streak 已是 2）。
                const int bonus = session->streak >= QPQ_STREAK_BONUS_AT
                                      ? QPQ_SCORE_STREAK_BONUS
                                      : 0;
                session->score += QPQ_SCORE_CORRECT + bonus;
                session->correct_count++;
                session->streak++;
                if (session->streak > session->max_streak) {
                    session->max_streak = session->streak;
                }
            } else {
                session->score -= QPQ_SCORE_WRONG_PENALTY;
                if (session->score < 0) session->score = 0;
                session->streak = 0;
            }

            if (session->history_count < (uint16_t)QPQ_RUN_LENGTH) {
                session->history[session->history_count].question = question;
                session->history[session->history_count].correct = correct;
                session->history_count++;
            }

            session->stage = QPQ_STAGE_REVEAL;
            return QPQ_ACT_ANSWERED;
        }

        case QPQ_STAGE_REVEAL:
            if (key != QPQ_KEY_OK && key != QPQ_KEY_BACK) return QPQ_ACT_NONE;
            session->position++;
            session->cursor = 0;
            session->chosen = 0;
            session->answered = false;
            if (session->position >= session->run_length) {
                session->stage = QPQ_STAGE_SUMMARY;
                return QPQ_ACT_FINISHED;
            }
            session->stage = QPQ_STAGE_ASK;
            return QPQ_ACT_ADVANCED;

        case QPQ_STAGE_SUMMARY:
            if (key == QPQ_KEY_OK) return qpq_session_start(session);
            if (key == QPQ_KEY_BACK) {
                session->stage = QPQ_STAGE_TITLE;
                return QPQ_ACT_LEFT;
            }
            return QPQ_ACT_NONE;

        default:
            return QPQ_ACT_NONE;
    }
}

const qpq_question_t *qpq_session_question(const qpq_session_t *session)
{
    if (!session) return NULL;
    if (session->stage != QPQ_STAGE_ASK && session->stage != QPQ_STAGE_REVEAL) {
        return NULL;
    }
    if (session->position >= session->run_length) return NULL;
    return qpq_question_at(session->order[session->position]);
}

uint16_t qpq_session_narration_clip(const qpq_session_t *session)
{
    if (!session) return UINT16_MAX;
    if (session->position >= session->run_length) return UINT16_MAX;
    if ((uint16_t)QPQ_AUDIO_NARRATION_COUNT == 0) return UINT16_MAX;
    const uint16_t index = session->order[session->position];
    if (index >= (uint16_t)QPQ_AUDIO_NARRATION_COUNT) return UINT16_MAX;
    return index;
}

bool qpq_session_is_correct(const qpq_session_t *session)
{
    if (!session || !session->answered) return false;
    const qpq_question_t *question = qpq_session_question(session);
    if (!question) return false;
    return session->chosen == question->answer;
}

int qpq_session_percent(const qpq_session_t *session)
{
    if (!session || session->run_length == 0) return 0;
    // 四舍五入，与网页版的 Math.round(correct / total * 100) 一致。
    return (int)(((uint32_t)session->correct_count * 100u + session->run_length / 2u) /
                 session->run_length);
}

const qpq_rank_t *qpq_session_rank(const qpq_session_t *session)
{
    return qpq_rank_for_percent(qpq_session_percent(session));
}
