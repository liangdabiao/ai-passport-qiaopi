// main/qpq_progress.h —— 跨局存档：已见题位图与历史最好成绩。
//
// 内容（题库、解析、方言配音）是固定的，值得留下来的只有「读者见过什么、做到过
// 什么」。这一份存档做两件事：
//
//   1. 记录哪些题已经出过 —— 下一局优先抽没见过的（见 qpq_session_set_seen）。
//      91 道题如果均匀随机，平均要抽近 20 局才能看全；有这张位图，三五局就能
//      把内容过一遍。
//   2. 记录历史最好成绩，让「再来一局」有个比较对象。
//
// 存档格式刻意严格：魔数 + 版本 + 校验和，任何一项不符就整体拒绝并回到空档，
// 而不是试图「修一修接着用」。NVS 掉电写坏是真实存在的，宁可丢一次成绩，也不
// 要把半截数据当成事实读进来 —— 这个取舍与拒绝路径都有宿主测试。
//
// 不依赖 ESP-IDF 与 LVGL；NVS 读写由 main/qpq_store.c 承担。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "qpq_session.h"
#include "qpq_text.h"

#define QPQ_PROGRESS_MAGIC 0x51505131u   // "QPQ1"
#define QPQ_PROGRESS_VERSION 1u

// 魔数 4 + 版本 2 + 预留 2 + 局数 4 + 最高分 4 + 最多答对 2 + 最长连对 2
// + 已见位图 QPQ_SEEN_BYTES + 校验和 4
#define QPQ_PROGRESS_BLOB_SIZE (24u + QPQ_SEEN_BYTES)

typedef enum {
    QPQ_PROGRESS_OK = 0,
    QPQ_PROGRESS_ERR_NULL,
    QPQ_PROGRESS_ERR_LENGTH,
    QPQ_PROGRESS_ERR_MAGIC,
    QPQ_PROGRESS_ERR_VERSION,
    QPQ_PROGRESS_ERR_CHECKSUM,
} qpq_progress_status_t;

typedef struct {
    uint32_t runs;            // 完成的局数
    int32_t best_score;       // 历史最高分
    uint16_t best_correct;    // 单局最多答对
    uint16_t best_streak;     // 历史最长连对
    uint8_t seen[QPQ_SEEN_BYTES];
} qpq_progress_t;

// 清空成一份空档（不是「不合法」，是合法的零值）。
void qpq_progress_reset(qpq_progress_t *progress);

// 已见题位图的读写。
bool qpq_progress_seen(const qpq_progress_t *progress, uint16_t question);
void qpq_progress_mark_seen(qpq_progress_t *progress, uint16_t question);
uint16_t qpq_progress_seen_count(const qpq_progress_t *progress);

// 把一局的结果并进存档：局数 +1，刷新三项最好成绩，并把本局出过的题标为已见。
// 传入的 session 应当已到结算阶段；未完成的局不该调用它。
void qpq_progress_record_run(qpq_progress_t *progress, const qpq_session_t *session);

// 序列化。返回写入字节数；容量不足返回 0（不写半截数据）。
uint32_t qpq_progress_serialize(const qpq_progress_t *progress,
                                uint8_t *out, uint32_t capacity);

// 反序列化。任何一项校验不过都返回对应错误，并且**不改动** progress：
// 调用方应当先 reset 再读，读到垃圾时保留空档。
qpq_progress_status_t qpq_progress_deserialize(qpq_progress_t *progress,
                                               const uint8_t *data, uint32_t length);

const char *qpq_progress_status_name(qpq_progress_status_t status);
