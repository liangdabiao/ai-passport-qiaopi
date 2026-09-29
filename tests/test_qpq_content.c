// tests/test_qpq_content.c —— 题库表的完整性与设备版式上限。
//
// 生成器已经在 Python 侧拦过一次超长内容，这里再从**设备侧**拿生成的表断言一次。
// 两道检查不是重复：Python 侧防的是「生成器被绕过」，这里防的是「生成的表与
// 设备实际版式脱节」——比如有人调窄了面板却没改预算。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qpq_content.h"
#include "qpq_text.h"
#include "qpq_wrap.h"

static void test_table_shape(void)
{
    assert(qpq_question_count() == QPQ_QUESTION_COUNT);
    assert(qpq_question_count() > 0);
    assert(qpq_question_at(0) != NULL);
    assert(qpq_question_at(qpq_question_count() - 1) != NULL);
    assert(qpq_question_at(qpq_question_count()) == NULL);

    assert(qpq_category_count() == QPQ_CATEGORY_COUNT);
    assert(qpq_category_at(0) != NULL);
    assert(qpq_category_at(qpq_category_count()) == NULL);

    for (uint8_t slot = 0; slot < (uint8_t)QPQ_ANSWER_SLOT_COUNT; slot++) {
        assert(qpq_answer_slot(slot) != NULL);
        assert(qpq_answer_slot(slot)[0] != '\0');
    }
    assert(qpq_answer_slot((uint8_t)QPQ_ANSWER_SLOT_COUNT) == NULL);
    puts("ok  表规模与边界访问");
}

static void test_every_question_is_well_formed(void)
{
    for (uint16_t index = 0; index < qpq_question_count(); index++) {
        const qpq_question_t *question = qpq_question_at(index);
        assert(question != NULL);

        assert(question->category != NULL && question->category[0] != '\0');
        assert(question->sentence != NULL && question->sentence[0] != '\0');
        assert(question->explain != NULL && question->explain[0] != '\0');
        assert(question->source != NULL && question->source[0] != '\0');
        assert(question->full != NULL && question->full[0] != '\0');

        // 正确项下标必须落在四个候选里。
        assert(question->answer < (uint8_t)QPQ_OPTION_COUNT);

        // 四个候选都非空，且互不相同。
        for (uint8_t slot = 0; slot < (uint8_t)QPQ_OPTION_COUNT; slot++) {
            assert(question->options[slot] != NULL);
            assert(question->options[slot][0] != '\0');
            for (uint8_t other = slot + 1; other < (uint8_t)QPQ_OPTION_COUNT; other++) {
                assert(strcmp(question->options[slot],
                              question->options[other]) != 0);
            }
        }

        // 正确项一定出现在完整原文里。不成立说明抽取串了行。
        assert(strstr(question->full, question->options[question->answer]) != NULL);
    }
    puts("ok  每道题字段完整、候选互异、正确项在原文里");
}

static void test_sentence_split_covers_whole_sentence(void)
{
    for (uint16_t index = 0; index < qpq_question_count(); index++) {
        const qpq_question_t *question = qpq_question_at(index);
        qpq_sentence_split_t split;
        assert(qpq_sentence_split(question, &split));

        // 前段 + 标记 + 后段 必须拼回原句，一个字节都不差。
        const size_t sentence_bytes = strlen(question->sentence);
        const size_t marker_bytes = strlen(QPQ_BLANK);
        assert((size_t)split.prefix_bytes + marker_bytes + strlen(split.suffix) ==
               sentence_bytes);
        assert(split.prefix == question->sentence);
        assert(memcmp(split.prefix, question->sentence, split.prefix_bytes) == 0);
        assert(split.suffix == question->sentence + split.prefix_bytes + marker_bytes);
    }
    puts("ok  填空位拆分可无损还原整句");
}

static void test_layout_budgets_hold_for_real_content(void)
{
    unsigned int worst_sentence = 0;
    unsigned int worst_option = 0;
    unsigned int worst_explain = 0;
    unsigned int worst_full = 0;
    unsigned int worst_source = 0;
    unsigned int worst_category = 0;

    for (uint16_t index = 0; index < qpq_question_count(); index++) {
        const qpq_question_t *question = qpq_question_at(index);

        // 句子按**填入正确答案之后**的长度算：判卷页显示的就是这个形态。
        const unsigned int filled = qpq_filled_length(question);
        assert(filled > 0);
        if (filled > worst_sentence) worst_sentence = filled;
        assert(filled <= (unsigned int)QPQ_SENTENCE_FILLED_MAX_CHARS);

        // 答题页显示的是槽位形态，字符数应当与源句相同。
        const unsigned int slot = qpq_utf8_length(question->sentence);
        assert(slot <= (unsigned int)QPQ_SENTENCE_MAX_CHARS);

        const unsigned int explain = qpq_utf8_length(question->explain);
        if (explain > worst_explain) worst_explain = explain;
        assert(explain <= (unsigned int)QPQ_EXPLAIN_MAX_CHARS);

        const unsigned int full = qpq_utf8_length(question->full);
        if (full > worst_full) worst_full = full;
        assert(full <= (unsigned int)QPQ_FULL_MAX_CHARS);

        const unsigned int source = qpq_utf8_length(question->source);
        if (source > worst_source) worst_source = source;
        assert(source <= (unsigned int)QPQ_SOURCE_MAX_CHARS);

        const unsigned int category = qpq_utf8_length(question->category);
        if (category > worst_category) worst_category = category;
        assert(category <= (unsigned int)QPQ_CATEGORY_MAX_CHARS);

        for (uint8_t slot = 0; slot < (uint8_t)QPQ_OPTION_COUNT; slot++) {
            const unsigned int option = qpq_utf8_length(question->options[slot]);
            if (option > worst_option) worst_option = option;
            assert(option <= (unsigned int)QPQ_OPTION_MAX_CHARS);
        }
    }

    printf("ok  版式上限全部成立（句子填空后 %u/%d、槽位 %u/%d，候选 %u/%d，解析 %u/%d，"
           "原文 %u/%d，出处 %u/%d，分类 %u/%d）\n",
           worst_sentence, QPQ_SENTENCE_FILLED_MAX_CHARS,
           worst_sentence, QPQ_SENTENCE_MAX_CHARS,
           worst_option, QPQ_OPTION_MAX_CHARS,
           worst_explain, QPQ_EXPLAIN_MAX_CHARS,
           worst_full, QPQ_FULL_MAX_CHARS,
           worst_source, QPQ_SOURCE_MAX_CHARS,
           worst_category, QPQ_CATEGORY_MAX_CHARS);
}

static void test_rank_thresholds(void)
{
    // 档位顺序与门槛要和网页版一致：>=90 大师，>=75 先贤，>=60 番客子弟，
    // >=40 初识侨批，否则需勤学。
    assert(strcmp(qpq_rank_for_percent(100)->rank, "侨批大师") == 0);
    assert(strcmp(qpq_rank_for_percent(90)->rank, "侨批大师") == 0);
    assert(strcmp(qpq_rank_for_percent(89)->rank, "识字先贤") == 0);
    assert(strcmp(qpq_rank_for_percent(75)->rank, "识字先贤") == 0);
    assert(strcmp(qpq_rank_for_percent(74)->rank, "番客子弟") == 0);
    assert(strcmp(qpq_rank_for_percent(60)->rank, "番客子弟") == 0);
    assert(strcmp(qpq_rank_for_percent(59)->rank, "初识侨批") == 0);
    assert(strcmp(qpq_rank_for_percent(40)->rank, "初识侨批") == 0);
    assert(strcmp(qpq_rank_for_percent(39)->rank, "需勤学") == 0);
    assert(strcmp(qpq_rank_for_percent(0)->rank, "需勤学") == 0);

    // 越界输入被夹取，不返回空指针。
    assert(qpq_rank_for_percent(-5) != NULL);
    assert(strcmp(qpq_rank_for_percent(-5)->rank, "需勤学") == 0);
    assert(qpq_rank_for_percent(1000) != NULL);
    assert(strcmp(qpq_rank_for_percent(1000)->rank, "侨批大师") == 0);

    // 每个档位都要有非空的副题与评语，结算页要显示它们。
    for (uint16_t index = 0; index < (uint16_t)QPQ_RANK_COUNT; index++) {
        assert(qpq_ranks[index].rank[0] != '\0');
        assert(qpq_ranks[index].subtitle[0] != '\0');
        assert(qpq_ranks[index].description[0] != '\0');
    }
    puts("ok  结算评级门槛与网页版一致，越界被夹取");
}

static void test_malformed_sentence_is_rejected(void)
{
    // 构造两个坏句子：没有标记、标记出现两次。函数必须返回 false，而不是猜。
    static const qpq_question_t no_marker = {
        .category = "思亲念家", .sentence = "自别慈颜", .options = {"甲", "乙", "丙", "丁"},
        .explain = "x", .source = "y", .full = "z", .answer = 0,
    };
    static const qpq_question_t two_markers = {
        .category = "思亲念家", .sentence = "自别____慈颜，时怀____。",
        .options = {"甲", "乙", "丙", "丁"},
        .explain = "x", .source = "y", .full = "z", .answer = 0,
    };
    qpq_sentence_split_t split;

    assert(!qpq_sentence_split(&no_marker, &split));
    assert(!qpq_sentence_split(&two_markers, &split));
    assert(!qpq_sentence_split(NULL, &split));
    assert(!qpq_sentence_split(&no_marker, NULL));
    assert(qpq_filled_length(&no_marker) == 0);
    puts("ok  坏句子被明确拒绝（无标记 / 两个标记 / 空指针）");
}

int main(void)
{
    test_table_shape();
    test_every_question_is_well_formed();
    test_sentence_split_covers_whole_sentence();
    test_layout_budgets_hold_for_real_content();
    test_rank_thresholds();
    test_malformed_sentence_is_rejected();
    puts("test_qpq_content: PASS");
    return 0;
}
