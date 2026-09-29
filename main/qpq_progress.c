// main/qpq_progress.c —— 见 qpq_progress.h。
// 不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#include "qpq_progress.h"

#include <stddef.h>

static void write_u16(uint8_t *at, uint16_t value)
{
    at[0] = (uint8_t)(value & 0xFFu);
    at[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void write_u32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value & 0xFFu);
    at[1] = (uint8_t)((value >> 8) & 0xFFu);
    at[2] = (uint8_t)((value >> 16) & 0xFFu);
    at[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint16_t read_u16(const uint8_t *at)
{
    return (uint16_t)((uint16_t)at[0] | ((uint16_t)at[1] << 8));
}

static uint32_t read_u32(const uint8_t *at)
{
    return (uint32_t)at[0] | ((uint32_t)at[1] << 8) |
           ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}

// Fletcher-32：够短、够快，能抓住单字节错与整段错位。这里不追求密码学强度，
// 要的是「半截写入」这类错误的确定性检出。
static uint32_t checksum_of(const uint8_t *data, uint32_t length)
{
    uint32_t low = 0xFFFFu;
    uint32_t high = 0xFFFFu;
    for (uint32_t i = 0; i < length; i++) {
        low = (low + data[i]) % 65535u;
        high = (high + low) % 65535u;
    }
    return (high << 16) | low;
}

void qpq_progress_reset(qpq_progress_t *progress)
{
    if (!progress) return;
    for (size_t i = 0; i < sizeof(*progress); i++) ((uint8_t *)progress)[i] = 0;
}

bool qpq_progress_seen(const qpq_progress_t *progress, uint16_t question)
{
    if (!progress) return false;
    if (question >= (uint16_t)QPQ_QUESTION_COUNT) return false;
    return (progress->seen[question >> 3] & (uint8_t)(1u << (question & 7))) != 0;
}

void qpq_progress_mark_seen(qpq_progress_t *progress, uint16_t question)
{
    if (!progress) return;
    if (question >= (uint16_t)QPQ_QUESTION_COUNT) return;
    progress->seen[question >> 3] |= (uint8_t)(1u << (question & 7));
}

uint16_t qpq_progress_seen_count(const qpq_progress_t *progress)
{
    if (!progress) return 0;
    uint16_t count = 0;
    for (uint16_t index = 0; index < (uint16_t)QPQ_QUESTION_COUNT; index++) {
        if (qpq_progress_seen(progress, index)) count++;
    }
    return count;
}

void qpq_progress_record_run(qpq_progress_t *progress, const qpq_session_t *session)
{
    if (!progress || !session) return;
    if (session->run_length == 0) return;

    progress->runs++;
    if (session->score > progress->best_score) {
        progress->best_score = session->score;
    }
    if (session->correct_count > progress->best_correct) {
        progress->best_correct = session->correct_count;
    }
    if (session->max_streak > progress->best_streak) {
        progress->best_streak = session->max_streak;
    }
    for (uint16_t i = 0; i < session->run_length && i < (uint16_t)QPQ_RUN_LENGTH; i++) {
        qpq_progress_mark_seen(progress, session->order[i]);
    }
}

uint32_t qpq_progress_serialize(const qpq_progress_t *progress,
                                uint8_t *out, uint32_t capacity)
{
    if (!progress || !out) return 0;
    if (capacity < QPQ_PROGRESS_BLOB_SIZE) return 0;

    uint8_t *at = out;
    write_u32(at, QPQ_PROGRESS_MAGIC);
    write_u16(at + 4, (uint16_t)QPQ_PROGRESS_VERSION);
    write_u16(at + 6, 0);                       // 预留，恒为 0
    write_u32(at + 8, progress->runs);
    write_u32(at + 12, (uint32_t)progress->best_score);
    write_u16(at + 16, progress->best_correct);
    write_u16(at + 18, progress->best_streak);
    for (uint16_t i = 0; i < (uint16_t)QPQ_SEEN_BYTES; i++) {
        at[20 + i] = progress->seen[i];
    }
    const uint32_t body = QPQ_PROGRESS_BLOB_SIZE - 4u;
    write_u32(at + body, checksum_of(out, body));
    return QPQ_PROGRESS_BLOB_SIZE;
}

qpq_progress_status_t qpq_progress_deserialize(qpq_progress_t *progress,
                                               const uint8_t *data, uint32_t length)
{
    if (!progress || !data) return QPQ_PROGRESS_ERR_NULL;
    if (length != QPQ_PROGRESS_BLOB_SIZE) return QPQ_PROGRESS_ERR_LENGTH;
    if (read_u32(data) != QPQ_PROGRESS_MAGIC) return QPQ_PROGRESS_ERR_MAGIC;
    if (read_u16(data + 4) != (uint16_t)QPQ_PROGRESS_VERSION) {
        return QPQ_PROGRESS_ERR_VERSION;
    }
    const uint32_t body = QPQ_PROGRESS_BLOB_SIZE - 4u;
    if (read_u32(data + body) != checksum_of(data, body)) {
        return QPQ_PROGRESS_ERR_CHECKSUM;
    }

    // 全部校验通过后才写入调用方的结构体：失败路径绝不留下半截状态。
    qpq_progress_t loaded;
    loaded.runs = read_u32(data + 8);
    loaded.best_score = (int32_t)read_u32(data + 12);
    loaded.best_correct = read_u16(data + 16);
    loaded.best_streak = read_u16(data + 18);
    for (uint16_t i = 0; i < (uint16_t)QPQ_SEEN_BYTES; i++) {
        loaded.seen[i] = data[20 + i];
    }
    *progress = loaded;
    return QPQ_PROGRESS_OK;
}

const char *qpq_progress_status_name(qpq_progress_status_t status)
{
    switch (status) {
        case QPQ_PROGRESS_OK:           return "ok";
        case QPQ_PROGRESS_ERR_NULL:     return "空指针";
        case QPQ_PROGRESS_ERR_LENGTH:   return "长度不符";
        case QPQ_PROGRESS_ERR_MAGIC:    return "魔数不符";
        case QPQ_PROGRESS_ERR_VERSION:  return "版本不符";
        case QPQ_PROGRESS_ERR_CHECKSUM: return "校验和不符";
        default:                        return "未知错误";
    }
}
