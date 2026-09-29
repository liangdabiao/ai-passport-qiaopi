// tests/test_ddj_chapter.c —— 内容访问层：表的结构不变式、越界、中文数字。
#include <assert.h>
#include <string.h>

#include "ddj_chapter.h"
#include "ddj_text.h"

int main(void)
{
    assert(DDJ_TOTAL_CHAPTERS == 81);
    assert(DDJ_DAO_LAST_CHAPTER == 37);
    assert(ddj_chapter_total() == 81);
    assert(ddj_chapter_count() >= 1);
    assert(DDJ_CHAPTER_COUNT == ddj_chapter_count());

    // 每一章的结构不变式。加内容时这几个断言会替我们兜住「章号跳号」
    // 「卷号写反」「选项表没按 3 对齐」这类错误。
    int passage_sum = 0;
    int point_sum = 0;
    for (int i = 0; i < ddj_chapter_count(); i++) {
        const ddj_chapter_t *chapter = ddj_chapter_at(i);
        assert(chapter != NULL);
        assert(chapter->number == i + 1);
        assert(chapter->title != NULL && chapter->title[0] != '\0');
        assert(chapter->passage_count >= 1);
        assert(chapter->point_count >= 3 && chapter->point_count <= 5);
        assert(chapter->option_first == (uint16_t)(i * DDJ_PONDER_OPTION_COUNT));
        assert(chapter->question != NULL && chapter->question[0] != '\0');

        // 卷号必须和章号一致：1..37 道经，38..81 德经。
        const ddj_volume_t expected =
            chapter->number <= DDJ_DAO_LAST_CHAPTER ? DDJ_VOLUME_DAO : DDJ_VOLUME_DE;
        assert(chapter->volume == expected);

        for (int p = 0; p < chapter->passage_count; p++) {
            const char *text = ddj_chapter_passage(chapter, p);
            assert(text != NULL && text[0] != '\0');
        }
        for (int p = 0; p < chapter->point_count; p++) {
            const char *text = ddj_chapter_point(chapter, p);
            assert(text != NULL && text[0] != '\0');
            // 汉字最多 3 字节，所以「48 字」的字节上限是 144。真正按字数
            // 判的是生成阶段（gen_content.py），这里只兜住明显跑飞的长度。
            assert(strlen(text) <= (size_t)DDJ_POINT_MAX_CHARS * 3);
        }
        for (int slot = 0; slot < DDJ_PONDER_OPTION_COUNT; slot++) {
            const char *text = ddj_chapter_option(chapter, slot);
            assert(text != NULL && text[0] != '\0');
        }

        passage_sum += chapter->passage_count;
        point_sum += chapter->point_count;
    }
    assert(passage_sum == DDJ_PASSAGE_COUNT);
    assert(point_sum == DDJ_POINT_COUNT);
    assert(DDJ_OPTION_COUNT == DDJ_CHAPTER_COUNT * DDJ_PONDER_OPTION_COUNT);

    // 第一章的内容是固定的：它是这个应用的样板章，改动会立刻在这里显形。
    const ddj_chapter_t *first = ddj_chapter_by_number(1);
    assert(first != NULL);
    assert(first == ddj_chapter_at(0));
    assert(strcmp(first->title, "道可道") == 0);
    assert(first->volume == DDJ_VOLUME_DAO);
    assert(strcmp(ddj_chapter_passage(first, 0), "道可道，非常道。") == 0);
    assert(strcmp(ddj_chapter_passage(first, first->passage_count - 1),
                  "玄之又玄，众妙之门。") == 0);

    // 越界一律返回 NULL，不返回「碰巧在内存里的东西」。
    assert(ddj_chapter_at(-1) == NULL);
    assert(ddj_chapter_at(DDJ_CHAPTER_COUNT) == NULL);
    assert(ddj_chapter_by_number(0) == NULL);
    assert(ddj_chapter_by_number(DDJ_TOTAL_CHAPTERS + 1) == NULL);
    // 还没收录的章：第 2 章现在应当取不到。
    if (DDJ_CHAPTER_COUNT == 1) assert(ddj_chapter_by_number(2) == NULL);
    assert(ddj_chapter_passage(first, -1) == NULL);
    assert(ddj_chapter_passage(first, first->passage_count) == NULL);
    assert(ddj_chapter_point(first, -1) == NULL);
    assert(ddj_chapter_point(first, first->point_count) == NULL);
    assert(ddj_chapter_option(first, -1) == NULL);
    assert(ddj_chapter_option(first, DDJ_PONDER_OPTION_COUNT) == NULL);
    assert(ddj_chapter_passage(NULL, 0) == NULL);
    assert(ddj_chapter_point(NULL, 0) == NULL);
    assert(ddj_chapter_option(NULL, 0) == NULL);

    assert(strcmp(ddj_volume_name(DDJ_VOLUME_DAO), "道经") == 0);
    assert(strcmp(ddj_volume_name(DDJ_VOLUME_DE), "德经") == 0);

    // 中文数字：章题要读成「第一章」而不是「第 1 章」。
    char out[DDJ_CHINESE_NUMBER_CAPACITY];
    assert(ddj_chinese_number(1, out, sizeof(out)) && strcmp(out, "一") == 0);
    assert(ddj_chinese_number(9, out, sizeof(out)) && strcmp(out, "九") == 0);
    // 十位是 1 时不写「一十」。
    assert(ddj_chinese_number(10, out, sizeof(out)) && strcmp(out, "十") == 0);
    assert(ddj_chinese_number(11, out, sizeof(out)) && strcmp(out, "十一") == 0);
    assert(ddj_chinese_number(19, out, sizeof(out)) && strcmp(out, "十九") == 0);
    assert(ddj_chinese_number(20, out, sizeof(out)) && strcmp(out, "二十") == 0);
    assert(ddj_chinese_number(37, out, sizeof(out)) && strcmp(out, "三十七") == 0);
    // 最后一章：三个汉字共 9 字节，容量常量必须刚好放得下。
    assert(ddj_chinese_number(81, out, sizeof(out)) && strcmp(out, "八十一") == 0);
    assert(strlen(out) == 9);
    assert(ddj_chinese_number(99, out, sizeof(out)) && strcmp(out, "九十九") == 0);

    // 越界与容量不足：返回 false，且 out 一定是空串，不留半截字。
    assert(!ddj_chinese_number(0, out, sizeof(out)) && out[0] == '\0');
    assert(!ddj_chinese_number(-1, out, sizeof(out)) && out[0] == '\0');
    assert(!ddj_chinese_number(100, out, sizeof(out)) && out[0] == '\0');
    assert(!ddj_chinese_number(81, out, 9) && out[0] == '\0');
    assert(!ddj_chinese_number(81, NULL, sizeof(out)));
    // capacity 为 0：同上，只能断言返回值。
    assert(!ddj_chinese_number(1, out, 0));

    // 章号的中文说法「第X章」：首页条目、顶栏标题都用它。
    char label[DDJ_CHAPTER_LABEL_CAPACITY];
    assert(ddj_chapter_label(1, label, sizeof(label)) && strcmp(label, "第一章") == 0);
    assert(ddj_chapter_label(37, label, sizeof(label)) && strcmp(label, "第三十七章") == 0);
    assert(ddj_chapter_label(81, label, sizeof(label)) && strcmp(label, "第八十一章") == 0);
    // 最长形态：5 个汉字 15 字节，容量常量必须刚好放得下。
    assert(strlen(label) == 15);
    assert(DDJ_CHAPTER_LABEL_CAPACITY == 16);
    assert(!ddj_chapter_label(0, label, sizeof(label)) && label[0] == '\0');
    assert(!ddj_chapter_label(100, label, sizeof(label)) && label[0] == '\0');
    // 容量刚好差一个字节也不行。
    assert(!ddj_chapter_label(81, label, 15) && label[0] == '\0');
    assert(ddj_chapter_label(81, label, 16) && strcmp(label, "第八十一章") == 0);
    assert(!ddj_chapter_label(1, NULL, sizeof(label)));
    // capacity 为 0 时写不进任何东西，所以这条只能断言返回值 ——
    // 「失败时 out 一定是空串」这个承诺在这里天然做不到，不做无意义的断言。
    assert(!ddj_chapter_label(1, label, 0));

    // 顶栏标题：「第X章 章题」。
    char heading[DDJ_CHAPTER_HEADING_CAPACITY];
    assert(ddj_chapter_heading(first, heading, sizeof(heading)) &&
           strcmp(heading, "第一章 道可道") == 0);
    assert(!ddj_chapter_heading(NULL, heading, sizeof(heading)) && heading[0] == '\0');
    assert(!ddj_chapter_heading(first, heading, 1) && heading[0] == '\0');
    assert(!ddj_chapter_heading(first, NULL, sizeof(heading)));
    return 0;
}
