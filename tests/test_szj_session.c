// tests/test_szj_session.c —— 会话状态机:建会话、判分、推进、错题重练。
#include <assert.h>
#include <stddef.h>

#include "szj_quiz.h"
#include "szj_session.h"
#include "szj_text.h"

int main(void)
{
    szj_session_t s;

    // 非法课号建会话必须失败,并把会话置于「已结束」状态。
    assert(!szj_session_start_lesson(&s, -1));
    assert(szj_session_done(&s));
    assert(szj_session_total(&s) == 0);
    assert(!szj_session_start_lesson(&s, SZJ_STANZA_COUNT));
    assert(szj_session_done(&s));
    assert(!szj_session_start_lesson(NULL, 0));

    assert(szj_session_star_cap() == SZJ_QUESTIONS_PER_LESSON);

    // 一课 3 题,全部答对拿满星。
    assert(szj_session_start_lesson(&s, 0));
    assert(!szj_session_is_review(&s));
    assert(szj_session_total(&s) == SZJ_QUESTIONS_PER_LESSON);
    assert(szj_session_index(&s) == 0);
    assert(szj_session_correct(&s) == 0);
    assert(!szj_session_done(&s));
    assert(!szj_session_current_answered(&s));
    assert(szj_session_current(&s) != NULL);

    int step = 0;
    while (!szj_session_done(&s)) {
        const szj_question_t *q = szj_session_current(&s);
        assert(q != NULL);
        assert(szj_session_index(&s) == step);
        assert(szj_session_answer(&s, q->correct_slot));
        assert(szj_session_current_answered(&s));
        step++;
        assert(szj_session_next(&s) == (step < SZJ_QUESTIONS_PER_LESSON));
    }
    assert(step == SZJ_QUESTIONS_PER_LESSON);
    assert(szj_session_correct(&s) == SZJ_QUESTIONS_PER_LESSON);
    assert(szj_session_current(&s) == NULL);
    assert(!szj_session_next(&s));

    // 答错拿不到星;同一题重复提交只记第一次的结果。
    assert(szj_session_start_lesson(&s, 1));
    const szj_question_t *q0 = szj_session_current(&s);
    const int wrong_slot = (q0->correct_slot + 1) % SZJ_QUIZ_OPTIONS;
    assert(!szj_session_answer(&s, wrong_slot));
    assert(szj_session_answer(&s, q0->correct_slot));
    assert(szj_session_correct(&s) == 0);
    assert(szj_session_current_answered(&s));

    // 非法选项被拒绝,不影响会话。
    assert(!szj_session_answer(&s, -1));
    assert(!szj_session_answer(&s, SZJ_QUIZ_OPTIONS));
    assert(szj_session_correct(&s) == 0);
    assert(!szj_session_answer(NULL, 0));

    // 错题重练:以答错过的「下句」重建。
    int lines[3] = {5, 9, 401};
    assert(szj_session_start_review(&s, lines, 3));
    assert(szj_session_is_review(&s));
    assert(szj_session_total(&s) == 3);
    assert(!szj_session_done(&s));
    const szj_question_t *r = szj_session_current(&s);
    assert(r != NULL);
    assert(r->lesson == -1);
    assert(r->answer_line == 5);
    assert(r->prompt_line == 4);

    // 空列表建不起来,且失败后会话是「已结束」而不是残留旧状态。
    assert(!szj_session_start_review(&s, NULL, 3));
    assert(szj_session_done(&s));
    assert(!szj_session_start_review(&s, lines, 0));
    assert(szj_session_done(&s));
    assert(!szj_session_start_review(NULL, lines, 3));

    // 复习列表超过上限要截断,免得一次练习没完没了。
    int many[10];
    for (int i = 0; i < 10; i++) many[i] = i + 2;
    assert(szj_session_start_review(&s, many, 10));
    assert(szj_session_total(&s) == SZJ_SESSION_MAX_QUESTIONS);
    assert(szj_session_is_review(&s));

    // 复习会话也能正常走完。
    int answered = 0;
    while (!szj_session_done(&s)) {
        const szj_question_t *q = szj_session_current(&s);
        assert(q != NULL);
        szj_session_answer(&s, q->correct_slot);
        answered++;
        szj_session_next(&s);
    }
    assert(answered == SZJ_SESSION_MAX_QUESTIONS);
    assert(szj_session_correct(&s) == SZJ_SESSION_MAX_QUESTIONS);

    // 复习模式下整场都可能拿满星,所以星星上限仍按「一课」算。
    assert(szj_session_star_cap() == SZJ_QUESTIONS_PER_LESSON);
    return 0;
}
