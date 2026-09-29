// main/ddj_chapter.c —— 见 ddj_chapter.h。
#include "ddj_chapter.h"

#include <string.h>

#include "ddj_text.h"

// 汉字在 UTF-8 里固定 3 字节。按 3 字节为单位搬运，不靠 strlen 去猜
// 「一」和「十」谁长谁短。
#define CJK_BYTES 3

static const char *const DIGITS[] = {
    "", "一", "二", "三", "四", "五", "六", "七", "八", "九",
};

static const char TEN[] = "十";

int ddj_chapter_count(void)
{
    return DDJ_CHAPTER_COUNT;
}

int ddj_chapter_total(void)
{
    return DDJ_TOTAL_CHAPTERS;
}

const ddj_chapter_t *ddj_chapter_at(int index)
{
    if (index < 0 || index >= DDJ_CHAPTER_COUNT) return NULL;
    return &ddj_chapters[index];
}

const ddj_chapter_t *ddj_chapter_by_number(int number)
{
    for (int i = 0; i < DDJ_CHAPTER_COUNT; i++) {
        if (ddj_chapters[i].number == number) return &ddj_chapters[i];
    }
    return NULL;
}

const char *ddj_chapter_passage(const ddj_chapter_t *chapter, int passage)
{
    if (!chapter) return NULL;
    if (passage < 0 || passage >= (int)chapter->passage_count) return NULL;
    const int index = (int)chapter->passage_first + passage;
    if (index < 0 || index >= DDJ_PASSAGE_COUNT) return NULL;
    return ddj_passages[index];
}

const char *ddj_chapter_point(const ddj_chapter_t *chapter, int point)
{
    if (!chapter) return NULL;
    if (point < 0 || point >= (int)chapter->point_count) return NULL;
    const int index = (int)chapter->point_first + point;
    if (index < 0 || index >= DDJ_POINT_COUNT) return NULL;
    return ddj_points[index];
}

const char *ddj_chapter_option(const ddj_chapter_t *chapter, int slot)
{
    if (!chapter) return NULL;
    if (slot < 0 || slot >= DDJ_PONDER_OPTION_COUNT) return NULL;
    const int index = (int)chapter->option_first + slot;
    if (index < 0 || index >= DDJ_OPTION_COUNT) return NULL;
    return ddj_options[index];
}

const char *ddj_volume_name(ddj_volume_t volume)
{
    return volume == DDJ_VOLUME_DE ? "德经" : "道经";
}

bool ddj_chinese_number(int number, char *out, size_t capacity)
{
    if (!out || capacity == 0) return false;
    out[0] = '\0';
    if (number < 1 || number > 99) return false;

    const int tens = number / 10;
    const int ones = number % 10;

    // 最长是「八十一」：三个汉字。先算准要几个字节，不够就直接拒绝，
    // 绝不写半个字出去。
    size_t needed = 1;                                  // 结尾的 NUL
    if (tens > 1) needed += CJK_BYTES;                  // 「八」
    if (tens > 0) needed += CJK_BYTES;                  // 「十」
    if (ones > 0) needed += CJK_BYTES;                  // 个位
    if (capacity < needed) return false;

    size_t written = 0;
    if (tens > 0) {
        // 十位是 1 时不写「一十」，直接写「十」。
        if (tens > 1) {
            memcpy(out + written, DIGITS[tens], CJK_BYTES);
            written += CJK_BYTES;
        }
        memcpy(out + written, TEN, CJK_BYTES);
        written += CJK_BYTES;
    }
    if (ones > 0) {
        memcpy(out + written, DIGITS[ones], CJK_BYTES);
        written += CJK_BYTES;
    }
    out[written] = '\0';
    return true;
}
