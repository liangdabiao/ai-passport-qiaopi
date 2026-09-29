// main/szj_quiz.c —— 见 szj_quiz.h。
#include "szj_quiz.h"

#include <stdint.h>
#include <string.h>

#include "szj_text.h"
#include "szj_text_util.h"

// 固定种子的线性同余发生器。只用于「稳定地挑干扰项」,不需要密码学强度。
static uint32_t next_rand(uint32_t *state) {
    *state = (*state) * 1664525u + 1013904223u;
    return (*state) >> 16;
}

static uint32_t question_seed(int lesson, int question) {
    return (uint32_t)(lesson * SZJ_QUESTIONS_PER_LESSON + question) * 2654435761u + 12345u;
}

// 干扰项必须:不与题干/答案同文案、不与题干同课(避免相邻句干扰)、与其他选项不同文案。
static bool candidate_usable(int candidate, int prompt_line, int answer_line,
                             const int *chosen, int chosen_count) {
    if (candidate < 0 || candidate >= SZJ_LINE_COUNT) return false;
    if (candidate == prompt_line || candidate == answer_line) return false;
    if (szj_stanza_of_line(candidate) == szj_stanza_of_line(prompt_line)) return false;

    const char *text = szj_lines[candidate];
    if (strcmp(text, szj_lines[prompt_line]) == 0) return false;
    if (strcmp(text, szj_lines[answer_line]) == 0) return false;
    for (int i = 0; i < chosen_count; i++) {
        if (strcmp(text, szj_lines[chosen[i]]) == 0) return false;
    }
    return true;
}

static int pick_distractor(uint32_t *state, int prompt_line, int answer_line,
                           const int *chosen, int chosen_count) {
    // 先随机试探,命中率足够高;失败太多就线性扫描兜底,保证一定返回合法值。
    for (int attempt = 0; attempt < 64; attempt++) {
        int candidate = (int)(next_rand(state) % (uint32_t)SZJ_LINE_COUNT);
        if (candidate_usable(candidate, prompt_line, answer_line, chosen, chosen_count)) {
            return candidate;
        }
    }
    const int offset = (int)(next_rand(state) % (uint32_t)SZJ_LINE_COUNT);
    for (int step = 0; step < SZJ_LINE_COUNT; step++) {
        const int candidate = (offset + step) % SZJ_LINE_COUNT;
        if (candidate_usable(candidate, prompt_line, answer_line, chosen, chosen_count)) {
            return candidate;
        }
    }
    return -1;
}

static bool build_from_pair(int prompt_line, int answer_line, int lesson, int question,
                            szj_question_t *out) {
    if (!out) return false;
    if (prompt_line < 0 || prompt_line >= SZJ_LINE_COUNT) return false;
    if (answer_line < 0 || answer_line >= SZJ_LINE_COUNT) return false;
    if (prompt_line == answer_line) return false;
    // 题干与选项不能同文案,否则题目本身有歧义。
    if (strcmp(szj_lines[prompt_line], szj_lines[answer_line]) == 0) return false;

    uint32_t state = question_seed(lesson < 0 ? answer_line : lesson, question);
    int chosen[SZJ_QUIZ_OPTIONS - 1];
    for (int i = 0; i < SZJ_QUIZ_OPTIONS - 1; i++) {
        const int picked = pick_distractor(&state, prompt_line, answer_line, chosen, i);
        if (picked < 0) return false;
        chosen[i] = picked;
    }

    szj_question_t result = {
        .lesson = lesson,
        .question = question,
        .prompt_line = prompt_line,
        .answer_line = answer_line,
        .correct_slot = (int)(next_rand(&state) % (uint32_t)SZJ_QUIZ_OPTIONS),
    };
    int distractor = 0;
    for (int slot = 0; slot < SZJ_QUIZ_OPTIONS; slot++) {
        result.options[slot] = (slot == result.correct_slot) ? answer_line : chosen[distractor++];
    }

    *out = result;
    return true;
}

int szj_quiz_question_count(int lesson) {
    if (lesson < 0 || lesson >= SZJ_STANZA_COUNT) return 0;
    return SZJ_QUESTIONS_PER_LESSON;
}

int szj_quiz_star_cap(void) {
    return SZJ_QUESTIONS_PER_LESSON;
}

bool szj_quiz_build(int lesson, int question, szj_question_t *out) {
    if (szj_quiz_question_count(lesson) == 0) return false;
    if (question < 0 || question >= SZJ_QUESTIONS_PER_LESSON) return false;

    const int first = lesson * SZJ_LINES_PER_STANZA;
    const int prompt_line = first + question;
    const int answer_line = prompt_line + 1;
    if (answer_line > first + SZJ_LINES_PER_STANZA - 1) return false;

    return build_from_pair(prompt_line, answer_line, lesson, question, out);
}

bool szj_quiz_build_review(int answer_line, int question, szj_question_t *out) {
    if (answer_line < 1 || answer_line >= SZJ_LINE_COUNT) return false;
    if (question < 0) return false;
    return build_from_pair(answer_line - 1, answer_line, -1, question, out);
}
