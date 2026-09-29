// tests/test_qpq_session.c —— 答题状态机与计分规则。
//
// 计分、连对加成、候选环绕、以及「每局 20 题不重复」这些规则，如果只靠手点一遍
// 来确认，改一次代码就得重点一遍。这里把它们变成断言：种子固定，结果就完全确定，
// 包括抽到哪 20 道题。
//
// 另外覆盖一处有意与网页版不同的地方：优先抽没见过的题。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qpq_audio.h"
#include "qpq_content.h"
#include "qpq_session.h"
#include "qpq_text.h"

static qpq_session_t s_session;

static void press_until_cursor(qpq_session_t *session, uint8_t choice)
{
    uint32_t guard = 0;
    while (session->cursor != choice && guard++ < 16) {
        qpq_session_key(session, QPQ_KEY_DOWN);
    }
    assert(session->cursor == choice);
}

static void answer_with_choice(qpq_session_t *session, uint8_t choice)
{
    const qpq_question_t *question = qpq_session_question(session);
    assert(question != NULL);
    press_until_cursor(session, choice);
    const qpq_action_t action = qpq_session_key(session, QPQ_KEY_OK);
    assert(action == QPQ_ACT_ANSWERED);
}

static void answer_correctly(qpq_session_t *session)
{
    const qpq_question_t *question = qpq_session_question(session);
    assert(question != NULL);
    answer_with_choice(session, question->answer);
}

static void answer_wrongly(qpq_session_t *session)
{
    const qpq_question_t *question = qpq_session_question(session);
    assert(question != NULL);
    answer_with_choice(
        session, (uint8_t)((question->answer + 1u) % (uint8_t)QPQ_OPTION_COUNT));
}

static void advance(qpq_session_t *session)
{
    const qpq_action_t action = qpq_session_key(session, QPQ_KEY_OK);
    assert(action == QPQ_ACT_ADVANCED || action == QPQ_ACT_FINISHED);
}

static void test_initial_state(void)
{
    qpq_session_init(&s_session, 1);
    assert(s_session.stage == QPQ_STAGE_TITLE);
    assert(s_session.score == 0);
    assert(qpq_session_question(&s_session) == NULL);
    assert(qpq_session_narration_clip(&s_session) == UINT16_MAX);

    // 标题页上／下无副作用，确定才开始。
    assert(qpq_session_key(&s_session, QPQ_KEY_UP) == QPQ_ACT_NONE);
    assert(qpq_session_key(&s_session, QPQ_KEY_DOWN) == QPQ_ACT_NONE);
    assert(s_session.stage == QPQ_STAGE_TITLE);
    puts("ok  初始在标题页，方向键无副作用");
}

static void test_start_draws_full_run(void)
{
    qpq_session_init(&s_session, 12345);
    assert(qpq_session_start(&s_session) == QPQ_ACT_STARTED);
    assert(s_session.stage == QPQ_STAGE_ASK);
    assert(s_session.run_length == (uint16_t)QPQ_RUN_LENGTH);
    assert(s_session.position == 0);
    assert(s_session.cursor == 0);

    // 一局之内不重复。抽 20 道不重复，是个真结论而不是概率——种子固定。
    bool used[QPQ_QUESTION_COUNT];
    memset(used, 0, sizeof(used));
    for (uint16_t i = 0; i < s_session.run_length; i++) {
        const uint16_t index = s_session.order[i];
        assert(index < (uint16_t)QPQ_QUESTION_COUNT);
        assert(!used[index]);
        used[index] = true;
    }
    puts("ok  开局抽满 20 题且一局内不重复");
}

static void test_same_seed_same_run(void)
{
    qpq_session_t first;
    qpq_session_t second;
    qpq_session_init(&first, 777);
    qpq_session_init(&second, 777);
    qpq_session_start(&first);
    qpq_session_start(&second);
    assert(memcmp(first.order, second.order, sizeof(first.order)) == 0);

    qpq_session_t third;
    qpq_session_init(&third, 778);
    qpq_session_start(&third);
    assert(memcmp(first.order, third.order, sizeof(first.order)) != 0);
    puts("ok  同种子同抽题、异种子异抽题（随机可复现）");
}

static void test_cursor_wraps_within_options(void)
{
    qpq_session_init(&s_session, 42);
    qpq_session_start(&s_session);

    assert(s_session.cursor == 0);
    assert(qpq_session_key(&s_session, QPQ_KEY_UP) == QPQ_ACT_MOVED);
    assert(s_session.cursor == (uint8_t)QPQ_OPTION_COUNT - 1);   // 0 向上绕到最后一格
    assert(qpq_session_key(&s_session, QPQ_KEY_DOWN) == QPQ_ACT_MOVED);
    assert(s_session.cursor == 0);                                // 再向下绕回 0
    for (uint8_t i = 0; i < (uint8_t)QPQ_OPTION_COUNT; i++) {
        assert(s_session.cursor == i);
        qpq_session_key(&s_session, QPQ_KEY_DOWN);
    }
    assert(s_session.cursor == 0);
    puts("ok  候选选中在四项之间环绕");
}

static void test_correct_and_wrong_scoring(void)
{
    qpq_session_init(&s_session, 100);
    qpq_session_start(&s_session);

    answer_correctly(&s_session);
    assert(s_session.score == 10);
    assert(s_session.correct_count == 1);
    assert(s_session.streak == 1);
    assert(s_session.max_streak == 1);
    assert(s_session.stage == QPQ_STAGE_REVEAL);
    assert(qpq_session_is_correct(&s_session));
    advance(&s_session);

    answer_wrongly(&s_session);
    assert(s_session.score == 10 - 3);      // 10 分基础上扣 3
    assert(s_session.correct_count == 1);
    assert(s_session.streak == 0);
    assert(!qpq_session_is_correct(&s_session));
    advance(&s_session);

    answer_wrongly(&s_session);
    assert(s_session.score == 4);           // 7 分基础上再扣 3
    advance(&s_session);

    answer_wrongly(&s_session);
    assert(s_session.score == 1);           // 4 - 3 = 1
    advance(&s_session);

    answer_wrongly(&s_session);
    assert(s_session.score == 0);           // 1 - 3 夹到 0，不为负
    puts("ok  答对得 10、答错扣 3 且分数不为负");
}

static void test_streak_bonus_matches_web(void)
{
    qpq_session_init(&s_session, 2024);
    qpq_session_start(&s_session);

    answer_correctly(&s_session);
    assert(s_session.score == 10);
    assert(s_session.streak == 1);
    advance(&s_session);

    answer_correctly(&s_session);
    assert(s_session.score == 20);
    assert(s_session.streak == 2);
    advance(&s_session);

    // 第三次连对才拿到加成：判卷时「自增之前」的连对数是 2。
    answer_correctly(&s_session);
    assert(s_session.score == 32);
    assert(s_session.streak == 3);
    assert(s_session.max_streak == 3);
    advance(&s_session);

    // 答错清零连对，之后的加成要重新攒。
    answer_wrongly(&s_session);
    assert(s_session.streak == 0);
    assert(s_session.max_streak == 3);
    puts("ok  连对加成门槛为 2，答错清零（与网页版一致）");
}

static void test_abandon_returns_to_title(void)
{
    qpq_session_init(&s_session, 55);
    qpq_session_start(&s_session);
    answer_correctly(&s_session);
    advance(&s_session);

    assert(qpq_session_key(&s_session, QPQ_KEY_BACK) == QPQ_ACT_LEFT);
    assert(s_session.stage == QPQ_STAGE_TITLE);
    assert(qpq_session_question(&s_session) == NULL);

    // 放弃后重新开始，计分归零。
    qpq_session_start(&s_session);
    assert(s_session.score == 0);
    assert(s_session.correct_count == 0);
    puts("ok  答题中放弃回到标题，重开计分归零");
}

static void test_full_run_reaches_summary(void)
{
    qpq_session_init(&s_session, 31337);
    qpq_session_start(&s_session);

    for (uint16_t i = 0; i < (uint16_t)QPQ_RUN_LENGTH; i++) {
        assert(s_session.position == i);
        answer_correctly(&s_session);
        const qpq_action_t action = qpq_session_key(&s_session, QPQ_KEY_OK);
        if (i + 1 < (uint16_t)QPQ_RUN_LENGTH) {
            assert(action == QPQ_ACT_ADVANCED);
            assert(s_session.stage == QPQ_STAGE_ASK);
        } else {
            assert(action == QPQ_ACT_FINISHED);
            assert(s_session.stage == QPQ_STAGE_SUMMARY);
        }
    }

    assert(s_session.correct_count == (uint16_t)QPQ_RUN_LENGTH);
    assert(s_session.history_count == (uint16_t)QPQ_RUN_LENGTH);
    assert(s_session.max_streak == (uint16_t)QPQ_RUN_LENGTH);
    assert(qpq_session_percent(&s_session) == 100);
    assert(strcmp(qpq_session_rank(&s_session)->rank, "侨批大师") == 0);
    assert(qpq_session_question(&s_session) == NULL);

    // 全对：前两题各 10 分，之后每題 12 分 => 20 + 18 x 12 = 236。
    assert(s_session.score == 10 + 10 + (int)(QPQ_RUN_LENGTH - 2) * 12);

    // 结算页确定重开；返回键回标题。
    assert(qpq_session_key(&s_session, QPQ_KEY_OK) == QPQ_ACT_STARTED);
    assert(s_session.stage == QPQ_STAGE_ASK);
    assert(s_session.score == 0);
    assert(qpq_session_key(&s_session, QPQ_KEY_BACK) == QPQ_ACT_LEFT);
    puts("ok  20 题跑到底进入结算，全对 100% 评「侨批大师」，确定性重开");
}

static void test_partial_score_percent_rounding(void)
{
    // 7/20 = 35% -> 需勤学；8/20 = 40% -> 初识侨批。四舍五入与网页版一致。
    qpq_session_init(&s_session, 9);
    qpq_session_start(&s_session);

    for (uint16_t i = 0; i < (uint16_t)QPQ_RUN_LENGTH; i++) {
        if (i < 7) {
            answer_correctly(&s_session);
        } else {
            answer_wrongly(&s_session);
        }
        qpq_session_key(&s_session, QPQ_KEY_OK);
    }
    assert(s_session.correct_count == 7);
    assert(qpq_session_percent(&s_session) == 35);
    assert(strcmp(qpq_session_rank(&s_session)->rank, "需勤学") == 0);
    puts("ok  正确率与评级（35% -> 需勤学）");
}

static void test_narration_clip_follows_question(void)
{
    qpq_session_init(&s_session, 888);
    qpq_session_start(&s_session);

    for (uint16_t i = 0; i < (uint16_t)QPQ_RUN_LENGTH; i++) {
        // 方言配音的片段号与题目下标一一对应，所以这里应当恒等于抽到的题目下标。
        assert(qpq_session_narration_clip(&s_session) == s_session.order[i]);
        assert(qpq_session_narration_clip(&s_session) <
               (uint16_t)QPQ_AUDIO_NARRATION_COUNT);
        answer_correctly(&s_session);
        qpq_session_key(&s_session, QPQ_KEY_OK);
    }
    // 结算页没有当前题，也就没有配音。
    assert(qpq_session_narration_clip(&s_session) == UINT16_MAX);
    puts("ok  当前题的方言配音片段号与题目一一对应");
}

static void test_unseen_questions_are_preferred(void)
{
    uint8_t seen[QPQ_SEEN_BYTES];
    memset(seen, 0xFF, sizeof(seen));       // 全部标为已见
    const uint16_t fresh[3] = {5, 40, 90};
    for (uint16_t i = 0; i < 3; i++) {
        seen[fresh[i] >> 3] &= (uint8_t)~(1u << (fresh[i] & 7));
    }

    qpq_session_init(&s_session, 20260929);
    qpq_session_set_seen(&s_session, seen);
    qpq_session_start(&s_session);

    // 三道没见过的题必须全部出现。
    for (uint16_t i = 0; i < 3; i++) {
        bool found = false;
        for (uint16_t position = 0; position < s_session.run_length; position++) {
            if (s_session.order[position] == fresh[i]) found = true;
        }
        assert(found);
    }

    // 没有任何已见位图时，行为退化为纯随机，仍然不重复。
    qpq_session_init(&s_session, 20260929);
    qpq_session_start(&s_session);
    bool used[QPQ_QUESTION_COUNT];
    memset(used, 0, sizeof(used));
    for (uint16_t i = 0; i < s_session.run_length; i++) {
        assert(!used[s_session.order[i]]);
        used[s_session.order[i]] = true;
    }
    puts("ok  已见题位图生效：没见过的题优先出");
}

static void test_degenerate_calls(void)
{
    // 空指针不该崩。
    qpq_session_init(NULL, 1);
    assert(qpq_session_start(NULL) == QPQ_ACT_NONE);
    assert(qpq_session_key(NULL, QPQ_KEY_OK) == QPQ_ACT_NONE);
    assert(qpq_session_question(NULL) == NULL);
    assert(qpq_session_narration_clip(NULL) == UINT16_MAX);
    assert(!qpq_session_is_correct(NULL));
    assert(qpq_session_percent(NULL) == 0);

    // 种子为 0 时退回到固定常量，不卡在 xorshift32 的零陷阱上。
    qpq_session_init(&s_session, 0);
    qpq_session_start(&s_session);
    assert(s_session.run_length == (uint16_t)QPQ_RUN_LENGTH);
    puts("ok  退化调用（空指针 / 零种子）行为明确");
}

int main(void)
{
    test_initial_state();
    test_start_draws_full_run();
    test_same_seed_same_run();
    test_cursor_wraps_within_options();
    test_correct_and_wrong_scoring();
    test_streak_bonus_matches_web();
    test_abandon_returns_to_title();
    test_full_run_reaches_summary();
    test_partial_score_percent_rounding();
    test_narration_clip_follows_question();
    test_unseen_questions_are_preferred();
    test_degenerate_calls();
    puts("test_qpq_session: PASS");
    return 0;
}
