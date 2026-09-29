// tests/test_szj_quiz.c —— 「接句闯关」的出题正确性与稳定性。
#include <assert.h>
#include <string.h>

#include "szj_quiz.h"
#include "szj_text.h"
#include "szj_text_util.h"

static bool options_contain(const szj_question_t *q, int line)
{
    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) {
        if (q->options[i] == line) return true;
    }
    return false;
}

static void same_question(const szj_question_t *a, const szj_question_t *b)
{
    assert(a->lesson == b->lesson);
    assert(a->question == b->question);
    assert(a->prompt_line == b->prompt_line);
    assert(a->answer_line == b->answer_line);
    assert(a->correct_slot == b->correct_slot);
    for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) assert(a->options[i] == b->options[i]);
}

int main(void)
{
    assert(szj_quiz_question_count(-1) == 0);
    assert(szj_quiz_question_count(SZJ_STANZA_COUNT) == 0);
    assert(szj_quiz_question_count(0) == SZJ_QUESTIONS_PER_LESSON);
    assert(szj_quiz_star_cap() == SZJ_QUESTIONS_PER_LESSON);
    assert(SZJ_QUIZ_OPTIONS == 3);

    // 越界调用必须失败,且不许写坏 out。
    assert(!szj_quiz_build(-1, 0, NULL));
    assert(!szj_quiz_build(0, -1, NULL));
    assert(!szj_quiz_build(0, SZJ_QUESTIONS_PER_LESSON, NULL));
    assert(!szj_quiz_build(SZJ_STANZA_COUNT, 0, NULL));

    // 逐课逐题全量校验:题干与答案相邻,选项三选一且互不相同。
    for (int lesson = 0; lesson < SZJ_STANZA_COUNT; lesson++) {
        for (int question = 0; question < SZJ_QUESTIONS_PER_LESSON; question++) {
            szj_question_t q = {0};
            assert(szj_quiz_build(lesson, question, &q));

            const int first = lesson * SZJ_LINES_PER_STANZA;
            assert(q.lesson == lesson);
            assert(q.question == question);
            assert(q.prompt_line == first + question);
            assert(q.answer_line == q.prompt_line + 1);
            assert(q.answer_line <= first + SZJ_LINES_PER_STANZA - 1);

            assert(q.correct_slot >= 0 && q.correct_slot < SZJ_QUIZ_OPTIONS);
            assert(q.options[q.correct_slot] == q.answer_line);
            assert(options_contain(&q, q.answer_line));

            for (int i = 0; i < SZJ_QUIZ_OPTIONS; i++) {
                assert(q.options[i] >= 0 && q.options[i] < SZJ_LINE_COUNT);
                // 干扰项不能与题干/答案同课,否则「接句」有歧义。
                assert(q.options[i] != q.prompt_line);
                for (int j = i + 1; j < SZJ_QUIZ_OPTIONS; j++) {
                    assert(q.options[i] != q.options[j]);
                    assert(strcmp(szj_line(q.options[i]), szj_line(q.options[j])) != 0);
                }
            }
        }
    }

    // 同一 (课, 题) 必须稳定复现,否则「错题重练」的答案会对不上。
    szj_question_t a = {0}, b = {0};
    assert(szj_quiz_build(7, 2, &a));
    assert(szj_quiz_build(7, 2, &b));
    same_question(&a, &b);

    // 相邻两句文案相同的课文会让题目自身有歧义;当前语料不含这种情况。
    for (int i = 0; i + 1 < SZJ_LINE_COUNT; i++) {
        assert(strcmp(szj_line(i), szj_line(i + 1)) != 0);
    }

    // 复习题:以某个「下句」重建,题干取它的上一句。
    szj_question_t r = {0};
    assert(szj_quiz_build_review(5, 0, &r));
    assert(r.lesson == -1);
    assert(r.prompt_line == 4);
    assert(r.answer_line == 5);
    assert(r.options[r.correct_slot] == 5);

    assert(!szj_quiz_build_review(0, 0, &r));
    assert(!szj_quiz_build_review(SZJ_LINE_COUNT, 0, &r));
    assert(!szj_quiz_build_review(5, -1, &r));
    assert(!szj_quiz_build_review(5, 0, NULL));
    return 0;
}
