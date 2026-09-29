// main/ddj_chapter.h —— 经文内容的访问层。
//
// 生成的 ddj_text.c 只负责把表摊开；这一层把「第几章、第几屏、第几条」翻成
// 指针，并在这里挡住越界。全是纯函数：不碰 ESP-IDF、不碰 LVGL，所以能在电脑上
// 直接测（tests/test_ddj_chapter.c）。
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ddj_text.h"

/* 「八十一」三个汉字各占 3 字节，加结尾 NUL 共 10 字节。 */
#define DDJ_CHINESE_NUMBER_CAPACITY 10

/* 已经收录了几章。原型阶段是 1，补齐后是 81。 */
int ddj_chapter_count(void);

/* 全本固定 81 章。进度按这个宽度存，与收录进度无关。 */
int ddj_chapter_total(void);

/* index 是 0 基下标，越界返回 NULL。 */
const ddj_chapter_t *ddj_chapter_at(int index);

/* 按章号（从 1 起）取。这一章还没收录时返回 NULL。 */
const ddj_chapter_t *ddj_chapter_by_number(int number);

/* 本章第 passage 屏原文；越界返回 NULL。 */
const char *ddj_chapter_passage(const ddj_chapter_t *chapter, int passage);

/* 本章第 point 条点拨；越界返回 NULL。 */
const char *ddj_chapter_point(const ddj_chapter_t *chapter, int point);

/* 本章参究的第 slot 个选项（0..2）；越界返回 NULL。 */
const char *ddj_chapter_option(const ddj_chapter_t *chapter, int slot);

/* 卷名：道经 / 德经。 */
const char *ddj_volume_name(ddj_volume_t volume);

/* 把 1..99 写成中文数字：「一」「十」「十一」「二十一」「八十一」。
 * 越界或 capacity 不够时返回 false，且 out 一定被写成空串。 */
bool ddj_chinese_number(int number, char *out, size_t capacity);
