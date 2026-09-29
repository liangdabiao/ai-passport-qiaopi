// main/qpq_content.c —— 见 qpq_content.h。
// 不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#include "qpq_content.h"

#include <string.h>

#include "qpq_wrap.h"

// 填空标记。定义在生成的 qpq_text.h 里，与生成器用的是同一份字面量。
static const char k_blank[] = QPQ_BLANK;

uint16_t qpq_question_count(void)
{
    return (uint16_t)QPQ_QUESTION_COUNT;
}

const qpq_question_t *qpq_question_at(uint16_t index)
{
    if (index >= (uint16_t)QPQ_QUESTION_COUNT) return NULL;
    return &qpq_questions[index];
}

uint16_t qpq_category_count(void)
{
    return (uint16_t)QPQ_CATEGORY_COUNT;
}

const char *qpq_category_at(uint16_t index)
{
    if (index >= (uint16_t)QPQ_CATEGORY_COUNT) return NULL;
    return qpq_categories[index];
}

const char *qpq_answer_slot(uint8_t index)
{
    if (index >= (uint8_t)QPQ_ANSWER_SLOT_COUNT) return NULL;
    return qpq_answer_slots[index];
}

const qpq_rank_t *qpq_rank_for_percent(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    for (uint16_t index = 0; index < (uint16_t)QPQ_RANK_COUNT; index++) {
        if (percent >= (int)qpq_ranks[index].percent) return &qpq_ranks[index];
    }
    // 生成的表最后一档下限是 0，正常不会走到这里；返回最后一档而不是 NULL，
    // 让调用方不必处理空指针。
    return &qpq_ranks[QPQ_RANK_COUNT - 1];
}

bool qpq_sentence_split(const qpq_question_t *question, qpq_sentence_split_t *out)
{
    if (!question || !out || !question->sentence) return false;

    const char *marker = strstr(question->sentence, k_blank);
    if (!marker) return false;
    // 标记必须恰好出现一次。多一次就说明句子被写坏了。
    if (strstr(marker + strlen(k_blank), k_blank)) return false;

    out->prefix = question->sentence;
    out->prefix_bytes = (uint16_t)(marker - question->sentence);
    out->suffix = marker + strlen(k_blank);
    return true;
}

uint16_t qpq_filled_length(const qpq_question_t *question)
{
    if (!question || !question->sentence) return 0;
    qpq_sentence_split_t split;
    if (!qpq_sentence_split(question, &split)) return 0;

    const char *answer = question->options[question->answer];
    if (!answer) return 0;
    // 字符数 = 前段 + 正确答案 + 后段。
    return (uint16_t)(qpq_utf8_length_prefix(split.prefix, split.prefix_bytes) +
                      qpq_utf8_length(answer) +
                      qpq_utf8_length(split.suffix));
}

// 四个全角下划线（U+FF3F）。用全角有两个理由：在中文字库里它与汉字等宽，
// 排版不会多出或少掉位置；而且四个连起来是一条实线，看起来就是「一个待填的空」。
// 半角下划线在中文行里会断断续续，还会让字符数与源句对不上。
#define QPQ_SLOT_TEXT "＿＿＿＿"

// 两个形态的构造只差中间插什么，所以共用一个实现。
static uint16_t build_sentence(const qpq_question_t *question, const char *middle,
                              char *out, size_t capacity)
{
    if (!out || capacity == 0) return 0;
    out[0] = '\0';

    qpq_sentence_split_t split;
    if (!qpq_sentence_split(question, &split)) return 0;

    const size_t middle_bytes = strlen(middle);
    const size_t suffix_bytes = strlen(split.suffix);
    const size_t total = (size_t)split.prefix_bytes + middle_bytes + suffix_bytes;
    if (total + 1 > capacity) return 0;   // 容量不足整体失败，不留半截

    memcpy(out, split.prefix, split.prefix_bytes);
    memcpy(out + split.prefix_bytes, middle, middle_bytes);
    memcpy(out + split.prefix_bytes + middle_bytes, split.suffix, suffix_bytes + 1);
    return (uint16_t)total;
}

uint16_t qpq_sentence_slot(const qpq_question_t *question, char *out, size_t capacity)
{
    return build_sentence(question, QPQ_SLOT_TEXT, out, capacity);
}

uint16_t qpq_sentence_filled(const qpq_question_t *question, char *out, size_t capacity)
{
    if (!question) return 0;
    const char *answer = question->options[question->answer];
    if (!answer) return 0;
    return build_sentence(question, answer, out, capacity);
}
