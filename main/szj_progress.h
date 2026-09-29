// main/szj_progress.h —— 学习进度的纯数据模型与序列化。
//
// 只做「算」,不做「存」:NVS 读写由 szj_store 负责。整个模型可以在宿主上测试,
// 包括序列化格式的往返、越界与损坏数据的拒绝。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "szj_text.h"

#define SZJ_LESSON_COUNT SZJ_STANZA_COUNT
#define SZJ_MAX_STARS_PER_LESSON 3
#define SZJ_WRONG_BYTES ((SZJ_LINE_COUNT + 7) / 8)

// 序列化格式:magic(1) version(1) card_line(2, 小端) stars(n) wrong(m) checksum(1)
#define SZJ_PROGRESS_MAGIC 0x53u   /* 'S' */
#define SZJ_PROGRESS_VERSION 1u
#define SZJ_PROGRESS_HEADER_BYTES 4
#define SZJ_PROGRESS_BLOB_SIZE \
    (SZJ_PROGRESS_HEADER_BYTES + SZJ_LESSON_COUNT + SZJ_WRONG_BYTES + 1)

typedef struct {
    uint8_t stars[SZJ_LESSON_COUNT];   // 每课 0..SZJ_MAX_STARS_PER_LESSON
    uint8_t wrong[SZJ_WRONG_BYTES];    // 答错过的「下句」位图
    uint16_t card_line;                // 认字卡上次读到第几句
} szj_progress_t;

// 清零,card_line 回到 0。
void szj_progress_reset(szj_progress_t *progress);

// 累计星星总数;progress 为 NULL 返回 0。
int szj_progress_total_stars(const szj_progress_t *progress);

// 已通关课数(至少拿到 1 颗星)。
int szj_progress_completed_lessons(const szj_progress_t *progress);

// 满星课数。
int szj_progress_perfect_lessons(const szj_progress_t *progress);

// 「该学哪一课」:第一门一颗星都没拿到的课;全部都拿过星就回到第 0 课复习。
int szj_progress_next_lesson(const szj_progress_t *progress);

// 记录一课的星星:只增不减(重练不会把已有星星降下来)。非法参数被忽略。
void szj_progress_record_lesson(szj_progress_t *progress, int lesson, int stars);

// 错题位图。
bool szj_progress_is_wrong(const szj_progress_t *progress, int line);
void szj_progress_mark_wrong(szj_progress_t *progress, int line);
void szj_progress_clear_wrong(szj_progress_t *progress, int line);
int szj_progress_wrong_count(const szj_progress_t *progress);

// 把错题按经文顺序导出到 out,最多 capacity 项;返回实际写入的项数。
int szj_progress_wrong_list(const szj_progress_t *progress, int *out, int capacity);

// 序列化。容量不足或参数非法返回 0。
size_t szj_progress_serialize(const szj_progress_t *progress, uint8_t *out, size_t capacity);

// 反序列化。magic/版本/长度/校验和任一不符即拒绝并返回 false,不改动 out。
bool szj_progress_deserialize(szj_progress_t *out, const uint8_t *data, size_t length);
