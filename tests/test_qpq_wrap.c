// tests/test_qpq_wrap.c —— 折行器，拿**真实题库**断言。
//
// 这个测试故意连上 qpq_content 与 qpq_text，断言的不是「函数在合成输入上正确」，
// 而是「屏幕上那句话本身放得下」。前者可以全绿而设备照样溢出；后者才是「一屏放
// 得下」这句话的证据。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qpq_content.h"
#include "qpq_text.h"
#include "qpq_wrap.h"

// 与 main/qpq_ui.c 的版式常量保持一致的字数预算。
#define PER_LINE_SENTENCE 8    // 24px 正文
#define PER_LINE_BODY 8        // 24px 正文（解析、完整原文、候选）
#define PER_LINE_SMALL 13      // 16px（出处、分类）

// 句子在答题页只有 3 行的高度（3 x 29 = 87px）。
#define LINES_SENTENCE 3

static char s_wrapped[QPQ_WRAP_CAPACITY];
static char s_sentence[128];

// —— 小工具：直接对折行结果做统计，不依赖被测模块的任何内部状态 ——

static unsigned int count_lines(const char *text)
{
    if (!text || !*text) return 0;
    unsigned int lines = 1;
    for (const char *at = text; *at; at++) {
        if (*at == '\n') lines++;
    }
    return lines;
}

static unsigned int max_line_chars(const char *text)
{
    unsigned int worst = 0;
    unsigned int current = 0;
    for (const char *at = text; *at; at++) {
        if (*at == '\n') {
            if (current > worst) worst = current;
            current = 0;
            continue;
        }
        // 只数 UTF-8 首字节，等价于数字符。
        if (((unsigned char)*at & 0xC0u) != 0x80u) current++;
    }
    if (current > worst) worst = current;
    return worst;
}

static bool is_closing_punctuation(unsigned int code)
{
    switch (code) {
        case 0xFF0Cu: case 0x3002u: case 0xFF01u: case 0xFF1Fu:
        case 0x3001u: case 0xFF1Bu: case 0xFF1Au: case 0xFF09u:
        case 0x300Du: case 0x300Fu: case 0x300Bu: case 0x3011u:
        case 0x2026u: case 0x2014u:
            return true;
        default:
            return false;
    }
}

static unsigned int first_codepoint(const char *at)
{
    const unsigned char *bytes = (const unsigned char *)at;
    if (bytes[0] < 0x80u) return bytes[0];
    if ((bytes[0] & 0xE0u) == 0xC0u) {
        return ((unsigned int)(bytes[0] & 0x1Fu) << 6) | (bytes[1] & 0x3Fu);
    }
    if ((bytes[0] & 0xF0u) == 0xE0u) {
        return ((unsigned int)(bytes[0] & 0x0Fu) << 12) |
               ((unsigned int)(bytes[1] & 0x3Fu) << 6) | (bytes[2] & 0x3Fu);
    }
    return ((unsigned int)(bytes[0] & 0x07u) << 18) |
           ((unsigned int)(bytes[1] & 0x3Fu) << 12) |
           ((unsigned int)(bytes[2] & 0x3Fu) << 6) | (bytes[3] & 0x3Fu);
}

static bool any_line_starts_with_closing_punctuation(const char *text)
{
    if (!text || !*text) return false;
    if (is_closing_punctuation(first_codepoint(text))) return true;
    for (const char *at = text; *at; at++) {
        if (*at == '\n' && at[1] != '\0') {
            if (is_closing_punctuation(first_codepoint(at + 1))) return true;
        }
    }
    return false;
}

static void build_filled_sentence(const qpq_question_t *question, char *out,
                                  size_t capacity)
{
    qpq_sentence_split_t split;
    assert(qpq_sentence_split(question, &split));
    const size_t prefix = split.prefix_bytes;
    const char *answer = question->options[question->answer];
    const size_t total = prefix + strlen(answer) + strlen(split.suffix);
    assert(total + 1 <= capacity);

    memcpy(out, split.prefix, prefix);
    strcpy(out + prefix, answer);
    strcat(out, split.suffix);
    assert(strlen(out) == total);
}

static void test_real_sentences_fit_three_lines(void)
{
    unsigned int worst_lines = 0;
    unsigned int worst_chars = 0;
    for (uint16_t index = 0; index < qpq_question_count(); index++) {
        const qpq_question_t *question = qpq_question_at(index);
        build_filled_sentence(question, s_sentence, sizeof(s_sentence));

        const size_t written = qpq_wrap_utf8(s_sentence, PER_LINE_SENTENCE,
                                             s_wrapped, sizeof(s_wrapped));
        assert(written > 0);

        // 一个字都不能丢：去掉换行后必须与原句逐字节相同。
        size_t compare = 0;
        for (size_t i = 0; i < written; i++) {
            if (s_wrapped[i] == '\n') continue;
            assert(s_wrapped[i] == s_sentence[compare]);
            compare++;
        }
        assert(compare == strlen(s_sentence));

        const unsigned int lines = count_lines(s_wrapped);
        const unsigned int chars = max_line_chars(s_wrapped);
        if (lines > worst_lines) worst_lines = lines;
        if (chars > worst_chars) worst_chars = chars;

        assert(lines <= LINES_SENTENCE);
        assert(chars <= PER_LINE_SENTENCE);
        assert(!any_line_starts_with_closing_punctuation(s_wrapped));
    }
    printf("ok  真实句子折行：最多 %u 行（预算 %d），单行最多 %u 字（预算 %d）\n",
           worst_lines, LINES_SENTENCE, worst_chars, PER_LINE_SENTENCE);
}

static void test_slot_form_also_fits_three_lines(void)
{
    // 答题页在选答案之前显示的是**槽位形态**（填空位换成四个全角下划线），
    // 不是源句里那几个半角下划线。它的字符数必须与源句完全相同，折行也必须
    // 同样放得进 3 行 —— 这两种形态的版式一旦分岔，就会出现「答题页放得下、
    // 判卷页溢出」这种只在半路上暴露的问题。
    unsigned int worst_lines = 0;
    unsigned int worst_chars = 0;
    for (uint16_t index = 0; index < qpq_question_count(); index++) {
        const qpq_question_t *question = qpq_question_at(index);
        char slot[128];
        const uint16_t written = qpq_sentence_slot(question, slot, sizeof(slot));
        assert(written > 0);
        assert(qpq_utf8_length(slot) == qpq_utf8_length(question->sentence));

        assert(qpq_wrap_utf8(slot, PER_LINE_SENTENCE, s_wrapped, sizeof(s_wrapped)) > 0);
        assert(count_lines(s_wrapped) <= LINES_SENTENCE);
        assert(max_line_chars(s_wrapped) <= PER_LINE_SENTENCE);
        assert(!any_line_starts_with_closing_punctuation(s_wrapped));
        if (count_lines(s_wrapped) > worst_lines) worst_lines = count_lines(s_wrapped);
        if (max_line_chars(s_wrapped) > worst_chars) worst_chars = max_line_chars(s_wrapped);
    }
    printf("ok  答题页的槽位形态：最多 %u 行、单行最多 %u 字（预算 %d 行 / %d 字）\n",
           worst_lines, worst_chars, LINES_SENTENCE, PER_LINE_SENTENCE);
}

static void test_slot_builder_rejects_bad_input(void)
{
    char slot[128];
    memset(slot, 0x7F, sizeof(slot));
    // 容量不足：整体失败，不留半截字符串。
    assert(qpq_sentence_slot(qpq_question_at(0), slot, 4) == 0);
    assert(slot[0] == '\0');
    // 空指针与零容量。
    assert(qpq_sentence_slot(NULL, slot, sizeof(slot)) == 0);
    assert(qpq_sentence_slot(qpq_question_at(0), NULL, sizeof(slot)) == 0);
    assert(qpq_sentence_slot(qpq_question_at(0), slot, 0) == 0);

    // 坏句子（没有填空标记）也被拒绝。
    static const qpq_question_t no_marker = {
        .category = "甲", .sentence = "自别慈颜", .options = {"甲", "乙", "丙", "丁"},
        .explain = "x", .source = "y", .full = "z", .answer = 0,
    };
    assert(qpq_sentence_slot(&no_marker, slot, sizeof(slot)) == 0);
    puts("ok  槽位构造器拒绝容量不足与坏句子");
}

static void test_real_prose_respects_line_width(void)
{
    unsigned int worst_explain = 0;
    unsigned int worst_full = 0;
    unsigned int worst_source = 0;

    for (uint16_t index = 0; index < qpq_question_count(); index++) {
        const qpq_question_t *question = qpq_question_at(index);

        assert(qpq_wrap_utf8(question->explain, PER_LINE_BODY,
                             s_wrapped, sizeof(s_wrapped)) > 0);
        if (max_line_chars(s_wrapped) > worst_explain) {
            worst_explain = max_line_chars(s_wrapped);
        }
        assert(max_line_chars(s_wrapped) <= PER_LINE_BODY);
        assert(!any_line_starts_with_closing_punctuation(s_wrapped));

        assert(qpq_wrap_utf8(question->full, PER_LINE_BODY,
                             s_wrapped, sizeof(s_wrapped)) > 0);
        if (max_line_chars(s_wrapped) > worst_full) {
            worst_full = max_line_chars(s_wrapped);
        }
        assert(max_line_chars(s_wrapped) <= PER_LINE_BODY);

        assert(qpq_wrap_utf8(question->source, PER_LINE_SMALL,
                             s_wrapped, sizeof(s_wrapped)) > 0);
        if (max_line_chars(s_wrapped) > worst_source) {
            worst_source = max_line_chars(s_wrapped);
        }
        assert(max_line_chars(s_wrapped) <= PER_LINE_SMALL);
    }
    printf("ok  解析/原文单行最多 %u / %u 字（预算 %d），出处最多 %u 字（预算 %d）\n",
           worst_explain, worst_full, PER_LINE_BODY, worst_source, PER_LINE_SMALL);
}

static void test_options_and_categories_are_one_line(void)
{
    for (uint16_t index = 0; index < qpq_question_count(); index++) {
        const qpq_question_t *question = qpq_question_at(index);

        assert(qpq_wrap_utf8(question->category, PER_LINE_SMALL,
                             s_wrapped, sizeof(s_wrapped)) > 0);
        assert(count_lines(s_wrapped) == 1);

        for (uint8_t slot = 0; slot < (uint8_t)QPQ_OPTION_COUNT; slot++) {
            assert(qpq_wrap_utf8(question->options[slot], PER_LINE_BODY,
                                 s_wrapped, sizeof(s_wrapped)) > 0);
            assert(count_lines(s_wrapped) == 1);
            assert(max_line_chars(s_wrapped) <= PER_LINE_BODY);
        }
    }
    puts("ok  分类与四个候选都只占一行");
}

static void test_capacity_failure_is_all_or_nothing(void)
{
    char tiny[8];
    memset(tiny, 0x7F, sizeof(tiny));
    // 容量不足：整体失败，不留半截字符串。
    assert(qpq_wrap_utf8("自别慈颜，时怀", 8, tiny, 4) == 0);
    assert(tiny[0] == '\0');

    // 容量为 0 与空指针：明确失败，不写内存。
    assert(qpq_wrap_utf8("自别慈颜，时怀", 8, tiny, 0) == 0);
    assert(qpq_wrap_utf8("自别慈颜，时怀", 8, NULL, 16) == 0);
    assert(qpq_wrap_utf8(NULL, 8, tiny, sizeof(tiny)) == 0);
    assert(tiny[0] == '\0');

    // 每行 1 字：退化但必须不崩、不丢字。
    const char *text = "自别慈颜";
    const unsigned int chars = qpq_utf8_length(text);
    assert(chars == 4);
    const size_t written = qpq_wrap_utf8(text, 1, s_wrapped, sizeof(s_wrapped));
    assert(written > 0);
    assert(count_lines(s_wrapped) == chars);
    assert(max_line_chars(s_wrapped) == 1);
    puts("ok  容量不足整体失败；每行 1 字不丢字不崩");
}

static void test_utf8_length(void)
{
    assert(qpq_utf8_length("") == 0);
    assert(qpq_utf8_length(NULL) == 0);
    assert(qpq_utf8_length("abc") == 3);
    assert(qpq_utf8_length("自别慈颜") == 4);
    assert(qpq_utf8_length("自别慈颜，时怀____。") == 12);
    assert(qpq_utf8_length("A自1") == 3);

    assert(qpq_utf8_length_prefix("自别慈颜", 0) == 0);
    assert(qpq_utf8_length_prefix("自别慈颜", 3) == 1);
    assert(qpq_utf8_length_prefix("自别慈颜", 4) == 1);  // 半个字符不算
    assert(qpq_utf8_length_prefix("自别慈颜", 6) == 2);
    assert(qpq_utf8_length_prefix("自别慈颜", 100) == 4);
    assert(qpq_utf8_length_prefix("abc", 2) == 2);
    assert(qpq_utf8_length_prefix(NULL, 4) == 0);
    puts("ok  UTF-8 字符计数（含前缀计数）");
}

int main(void)
{
    test_real_sentences_fit_three_lines();
    test_slot_form_also_fits_three_lines();
    test_slot_builder_rejects_bad_input();
    test_real_prose_respects_line_width();
    test_options_and_categories_are_one_line();
    test_capacity_failure_is_all_or_nothing();
    test_utf8_length();
    puts("test_qpq_wrap: PASS");
    return 0;
}
