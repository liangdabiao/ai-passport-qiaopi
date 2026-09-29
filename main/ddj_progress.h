// main/ddj_progress.h —— 读经进度：每章读到哪、怎么表态、有没有收下。
//
// 三件刻意的设计：
//  1. 宽度按全本 81 章写死（DDJ_TOTAL_CHAPTERS），与「目前收录了几章」无关。
//     以后补到 81 章时，存档格式一个字节都不用改。
//  2. 表态只存槽位（0/1/2），不存文案。文案在内容文件里按章定制，槽位的含义
//     由 ddj_slot_t 定义，温故与复习节奏只看槽位。
//  3. 序列化带 magic + version + 校验和，反序列化对越界值一律拒绝 —— 损坏的
//     存档被丢掉重来，不会被当成合法进度。
//
// 纯逻辑：不碰 NVS、不碰 LVGL，可以在电脑上测（tests/test_ddj_progress.c）。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ddj_text.h"

/* 参究表态。数值直接落盘，改顺序等于改存档格式。 */
typedef enum {
    DDJ_SLOT_NONE = 0,    /* 还没表态 */
    DDJ_SLOT_LANDED = 1,  /* 槽 0：接了 */
    DDJ_SLOT_CHEWING = 2, /* 槽 1：还要想 */
    DDJ_SLOT_MISSED = 3,  /* 槽 2：没接上 */
    DDJ_SLOT_COUNT = 4,
} ddj_slot_t;

#define DDJ_NOTES_BYTES  DDJ_TOTAL_CHAPTERS
#define DDJ_BITMAP_BYTES ((DDJ_TOTAL_CHAPTERS + 7) / 8)

#define DDJ_PROGRESS_MAGIC        0x44u /* 'D' */
#define DDJ_PROGRESS_VERSION      1u
#define DDJ_PROGRESS_HEADER_BYTES 4u /* magic + version + 2 字节日课次数 */
#define DDJ_PROGRESS_BLOB_SIZE                                                     \
    (DDJ_PROGRESS_HEADER_BYTES + DDJ_NOTES_BYTES + 2u * DDJ_BITMAP_BYTES + 1u)

typedef struct {
    uint16_t sessions;                  /* 完成过几次日课 */
    uint8_t notes[DDJ_NOTES_BYTES];     /* 每章表态，取值为 ddj_slot_t */
    uint8_t read[DDJ_BITMAP_BYTES];     /* 每章是否读过 */
    uint8_t starred[DDJ_BITMAP_BYTES];  /* 每章是否收下 */
} ddj_progress_t;

/* index 是 0 基章下标，合法范围 0..80。 */
bool ddj_progress_index_valid(int index);

void ddj_progress_reset(ddj_progress_t *progress);

ddj_slot_t ddj_progress_slot(const ddj_progress_t *progress, int index);
void ddj_progress_set_slot(ddj_progress_t *progress, int index, ddj_slot_t slot);

bool ddj_progress_has_read(const ddj_progress_t *progress, int index);
void ddj_progress_mark_read(ddj_progress_t *progress, int index);

bool ddj_progress_is_starred(const ddj_progress_t *progress, int index);
void ddj_progress_set_starred(ddj_progress_t *progress, int index, bool starred);

int ddj_progress_read_count(const ddj_progress_t *progress);
int ddj_progress_starred_count(const ddj_progress_t *progress);
int ddj_progress_sessions(const ddj_progress_t *progress);

/* 「待参」：表态是「还要想」或「没接上」的章。
 *
 * 这是「待参」那一页的数据来源。之所以不另做一个「收藏」动作：硬件只有三个键，
 * 而且在日课的四层里每个键都已经有明确职责，再塞一个收藏进去必然和某一个冲突。
 * 而「待参」不需要任何新输入 —— 它就是用户已经在参究里表过的态，语义上也正是
 * 这个应用「反复参」的落点。
 *
 * chapter_count 是已收录的章数；传 0 或负数时返回 0（或 -1）。 */
int ddj_progress_pending_count(const ddj_progress_t *progress, int chapter_count);

/* 第 rank 个待参章的章下标（0 基，按章号升序）；越界返回 -1。 */
int ddj_progress_pending_at(const ddj_progress_t *progress, int chapter_count, int rank);

/* 一次日课读完之后调用：日课次数 +1。 */
void ddj_progress_end_session(ddj_progress_t *progress);

/* 下一章该读哪一章（0 基下标）：第一本没读过的；都读过就从第 1 章再走一遍。
 * chapter_count 是已经收录的章数；为 0 时返回 -1。 */
int ddj_progress_next_chapter(const ddj_progress_t *progress, int chapter_count);

/* 槽位的中文说法，用于「上次你选了……」。非法槽位返回「未表态」。 */
const char *ddj_slot_name(int slot);

/* 写进 out，返回实际长度；容量不足或参数非法时返回 0。 */
size_t ddj_progress_serialize(const ddj_progress_t *progress, uint8_t *out, size_t capacity);

/* 解析存档。长度、magic、版本、校验和、取值范围任一项不对就返回 false，
 * 且不修改 *out。 */
bool ddj_progress_deserialize(ddj_progress_t *out, const uint8_t *data, size_t length);
