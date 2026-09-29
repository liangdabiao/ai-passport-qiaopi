// tests/test_ddj_wrap.c —— 折行：断点位置、禁则、容量，以及
// 「真实内容在真实预算下都不超行宽」这一条最要紧的不变量。
//
// 这个测试同时链接 ddj_chapter/ddj_text，所以断言的是**实际会出现在屏上的
// 那批字**，而不是测试里另抄一份的样本 —— 内容一改，这里立刻会知道。
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "ddj_chapter.h"
#include "ddj_wrap.h"

// 屏上真实的每行字数预算，与 ddj_ui.h 里的 DDJ_CHARS_* 保持一致：
//   原文 32px / 点拨·选项 24px / 参究问题 16px，内容区宽 210px
#define BUDGET_PASSAGE  6
#define BUDGET_POINT    8
#define BUDGET_OPTION   8
#define BUDGET_QUESTION 13

// 按 UTF-8 数一遍字符数（不计换行），用来验证折行不丢字。
static int count_chars_without_newlines(const char *text)
{
    int count = 0;
    for (const unsigned char *at = (const unsigned char *)text; *at; at++) {
        // 只在首字节上计数：多字节序列的后续字节都是 10xxxxxx。
        if ((*at & 0xC0u) != 0x80u && *at != '\n') count++;
    }
    return count;
}

static int count_lines(const char *text)
{
    int lines = text[0] ? 1 : 0;
    for (const char *at = text; *at; at++) {
        if (*at == '\n') lines++;
    }
    return lines;
}

// 最长那一行有几个字。版式预算就是靠这个被守住的。
static int max_line_chars(const char *text)
{
    int longest = 0;
    int current = 0;
    for (const unsigned char *at = (const unsigned char *)text; *at; at++) {
        if (*at == '\n') {
            if (current > longest) longest = current;
            current = 0;
            continue;
        }
        if ((*at & 0xC0u) == 0x80u) continue; // 续字节
        current++;
    }
    return current > longest ? current : longest;
}

// 禁则：任何一行的行首都不能是收尾标点。
static bool any_line_starts_with_closing_punctuation(const char *text)
{
    static const char *const BAD[] = {
        "，", "。", "！", "？", "、", "；", "：", "）", "」", "』", "》", "】", "…", "—",
    };
    for (const char *at = text; *at; at++) {
        if (*at != '\n') continue;
        const char *line = at + 1;
        for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
            if (strncmp(line, BAD[i], strlen(BAD[i])) == 0) return true;
        }
    }
    return false;
}

int main(void)
{
    char out[DDJ_WRAP_CAPACITY];

    // ---- 断点位置 ----
    // 每行 6 字：8 个字的句子折成 2 行，第一行满 6 个。
    assert(ddj_wrap_utf8("道可道，非常道。", 6, out, sizeof(out)) == 25);
    assert(strcmp(out, "道可道，非常\n道。") == 0);
    assert(count_lines(out) == 2);

    // 正好占满一行、后面没有字了，不补尾换行。
    assert(ddj_wrap_utf8("天地玄黄宇宙", 6, out, sizeof(out)) > 0);
    assert(strcmp(out, "天地玄黄宇宙") == 0);
    assert(count_lines(out) == 1);

    // 再多一个字才换行。
    assert(ddj_wrap_utf8("天地玄黄宇宙洪", 6, out, sizeof(out)) > 0);
    assert(strcmp(out, "天地玄黄宇宙\n洪") == 0);

    // ---- 禁则走的是「提前一字换行」，不是「把标点提上一行」----
    // 「。」正好落在第 9 个字上：提前在第 7 字后换行，两行都不超宽。
    assert(ddj_wrap_utf8("天地玄黄宇宙洪荒。", 8, out, sizeof(out)) > 0);
    assert(strcmp(out, "天地玄黄宇宙洪\n荒。") == 0);
    assert(count_lines(out) == 2);
    assert(max_line_chars(out) == 7);

    // 逗号、分号同样不上行首，且每一行都不超过每行字数。
    assert(ddj_wrap_utf8("故常无欲，以观其妙；常有欲，以观其徼。", 6, out, sizeof(out)) > 0);
    assert(strcmp(out, "故常无欲，以\n观其妙；常有\n欲，以观其\n徼。") == 0);
    assert(max_line_chars(out) <= 6);
    assert(!any_line_starts_with_closing_punctuation(out));

    // 收尾标点自己落在断点上时，不触发提前换行 —— 否则「，。」会被拆到两行，
    // 行首反而还是标点。
    assert(ddj_wrap_utf8("天地玄黄，，", 4, out, sizeof(out)) > 0);
    assert(strcmp(out, "天地玄\n黄，，") == 0);
    assert(!any_line_starts_with_closing_punctuation(out));

    // 已记录的退化输入：连续两个收尾标点卡在换行点，且换行点就是标点自己，
    // 提前换行无从下手，标点仍会落在行首。本项目的正文不出现这种串，
    // 这里把行为钉住，免得日后被当成 bug 反复排查。
    assert(ddj_wrap_utf8("天，，，", 2, out, sizeof(out)) > 0);
    assert(count_lines(out) == 2);
    assert(any_line_starts_with_closing_punctuation(out));

    // ---- 真实内容：逐条过预算 ----
    // 这是本文件最要紧的一段。它直接回答「一屏放得下吗」：折完之后
    // 最长的一行有没有超过该层的每行字数。
    const int chapters = ddj_chapter_count();
    assert(chapters > 0);

    for (int index = 0; index < chapters; index++) {
        const ddj_chapter_t *chapter = ddj_chapter_at(index);
        assert(chapter != NULL);

        for (int i = 0; i < (int)chapter->passage_count; i++) {
            const char *passage = ddj_chapter_passage(chapter, i);
            assert(passage != NULL);
            assert(ddj_wrap_utf8(passage, BUDGET_PASSAGE, out, sizeof(out)) > 0);
            assert(count_chars_without_newlines(out) == count_chars_without_newlines(passage));
            assert(max_line_chars(out) <= BUDGET_PASSAGE);
            assert(count_lines(out) <= 5); // 5 行 x 38px + 行距 <= 内容区 248px
            assert(!any_line_starts_with_closing_punctuation(out));
        }

        for (int i = 0; i < (int)chapter->point_count; i++) {
            const char *point = ddj_chapter_point(chapter, i);
            assert(point != NULL);
            assert(ddj_wrap_utf8(point, BUDGET_POINT, out, sizeof(out)) > 0);
            assert(count_chars_without_newlines(out) == count_chars_without_newlines(point));
            assert(max_line_chars(out) <= BUDGET_POINT);
            assert(count_lines(out) <= 7); // 7 行 x 29px + 行距 <= 内容区 248px
            assert(!any_line_starts_with_closing_punctuation(out));
        }

        for (int slot = 0; slot < 3; slot++) {
            const char *option = ddj_chapter_option(chapter, slot);
            assert(option != NULL);
            assert(ddj_wrap_utf8(option, BUDGET_OPTION, out, sizeof(out)) > 0);
            assert(max_line_chars(out) <= BUDGET_OPTION);
            // 选项是单行，折行后不该出现换行。
            assert(count_lines(out) == 1);
            assert(!any_line_starts_with_closing_punctuation(out));
        }

        assert(chapter->question != NULL && chapter->question[0] != '\0');
        assert(ddj_wrap_utf8(chapter->question, BUDGET_QUESTION, out, sizeof(out)) > 0);
        assert(count_chars_without_newlines(out) ==
               count_chars_without_newlines(chapter->question));
        assert(max_line_chars(out) <= BUDGET_QUESTION);
        assert(count_lines(out) <= 3);
        assert(!any_line_starts_with_closing_punctuation(out));
    }

    // ---- 参数与容量 ----
    assert(ddj_wrap_utf8("abcdefghij", 5, out, sizeof(out)) > 0);
    assert(strcmp(out, "abcde\nfghij") == 0);

    assert(ddj_wrap_utf8("短句", 8, out, sizeof(out)) > 0);
    assert(strcmp(out, "短句") == 0);

    // 每行 1 个字：退化但不死循环。
    assert(ddj_wrap_utf8("道可道", 1, out, sizeof(out)) > 0);
    assert(strcmp(out, "道\n可\n道") == 0);

    // 容量不足：返回 0，且不留半截字符串。
    assert(ddj_wrap_utf8("道可道，非常道。", 6, out, 6) == 0);
    assert(out[0] == '\0');
    assert(ddj_wrap_utf8("道可道，非常道。", 6, out, 1) == 0);
    assert(out[0] == '\0');
    // 就少一个字节也不行（25 字节正文 + NUL = 26）。
    assert(ddj_wrap_utf8("道可道，非常道。", 6, out, 25) == 0);
    assert(out[0] == '\0');
    assert(ddj_wrap_utf8("道可道，非常道。", 6, out, 26) == 25);

    // 参数非法。
    assert(ddj_wrap_utf8("道可道", 0, out, sizeof(out)) == 0);
    assert(ddj_wrap_utf8("道可道", -3, out, sizeof(out)) == 0);
    assert(ddj_wrap_utf8(NULL, 6, out, sizeof(out)) == 0);
    assert(out[0] == '\0');
    assert(ddj_wrap_utf8("道可道", 6, NULL, sizeof(out)) == 0);
    assert(ddj_wrap_utf8("道可道", 6, out, 0) == 0);
    assert(out[0] == '\0');

    // 空串：不写任何东西。
    assert(ddj_wrap_utf8("", 6, out, sizeof(out)) == 0);
    assert(out[0] == '\0');
    return 0;
}
