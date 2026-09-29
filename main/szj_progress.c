// main/szj_progress.c —— 见 szj_progress.h。
#include "szj_progress.h"

#include <string.h>

static uint8_t blob_checksum(const uint8_t *data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) sum += data[i];
    return (uint8_t)(sum ^ 0xA5u);
}

void szj_progress_reset(szj_progress_t *progress) {
    if (!progress) return;
    memset(progress, 0, sizeof(*progress));
}

int szj_progress_total_stars(const szj_progress_t *progress) {
    if (!progress) return 0;
    int total = 0;
    for (int i = 0; i < SZJ_LESSON_COUNT; i++) total += progress->stars[i];
    return total;
}

int szj_progress_completed_lessons(const szj_progress_t *progress) {
    if (!progress) return 0;
    int total = 0;
    for (int i = 0; i < SZJ_LESSON_COUNT; i++) {
        if (progress->stars[i] > 0) total++;
    }
    return total;
}

int szj_progress_perfect_lessons(const szj_progress_t *progress) {
    if (!progress) return 0;
    int total = 0;
    for (int i = 0; i < SZJ_LESSON_COUNT; i++) {
        if (progress->stars[i] >= SZJ_MAX_STARS_PER_LESSON) total++;
    }
    return total;
}

int szj_progress_next_lesson(const szj_progress_t *progress) {
    if (!progress) return 0;
    for (int i = 0; i < SZJ_LESSON_COUNT; i++) {
        if (progress->stars[i] == 0) return i;
    }
    // 每课都拿过星了,就从第一课开始复习。
    return 0;
}

void szj_progress_record_lesson(szj_progress_t *progress, int lesson, int stars) {
    if (!progress) return;
    if (lesson < 0 || lesson >= SZJ_LESSON_COUNT) return;
    if (stars < 0) stars = 0;
    if (stars > SZJ_MAX_STARS_PER_LESSON) stars = SZJ_MAX_STARS_PER_LESSON;
    if ((uint8_t)stars > progress->stars[lesson]) progress->stars[lesson] = (uint8_t)stars;
}

bool szj_progress_is_wrong(const szj_progress_t *progress, int line) {
    if (!progress) return false;
    if (line < 0 || line >= SZJ_LINE_COUNT) return false;
    return (progress->wrong[line / 8] & (uint8_t)(1u << (line % 8))) != 0;
}

void szj_progress_mark_wrong(szj_progress_t *progress, int line) {
    if (!progress) return;
    if (line < 0 || line >= SZJ_LINE_COUNT) return;
    progress->wrong[line / 8] |= (uint8_t)(1u << (line % 8));
}

void szj_progress_clear_wrong(szj_progress_t *progress, int line) {
    if (!progress) return;
    if (line < 0 || line >= SZJ_LINE_COUNT) return;
    progress->wrong[line / 8] &= (uint8_t)~(1u << (line % 8));
}

int szj_progress_wrong_count(const szj_progress_t *progress) {
    if (!progress) return 0;
    int total = 0;
    for (int i = 0; i < SZJ_LINE_COUNT; i++) {
        if (szj_progress_is_wrong(progress, i)) total++;
    }
    return total;
}

int szj_progress_wrong_list(const szj_progress_t *progress, int *out, int capacity) {
    if (!progress || !out || capacity <= 0) return 0;
    int count = 0;
    for (int i = 0; i < SZJ_LINE_COUNT && count < capacity; i++) {
        if (szj_progress_is_wrong(progress, i)) out[count++] = i;
    }
    return count;
}

size_t szj_progress_serialize(const szj_progress_t *progress, uint8_t *out, size_t capacity) {
    if (!progress || !out) return 0;
    if (capacity < SZJ_PROGRESS_BLOB_SIZE) return 0;

    size_t offset = 0;
    out[offset++] = SZJ_PROGRESS_MAGIC;
    out[offset++] = SZJ_PROGRESS_VERSION;
    out[offset++] = (uint8_t)(progress->card_line & 0xFFu);
    out[offset++] = (uint8_t)((progress->card_line >> 8) & 0xFFu);
    memcpy(out + offset, progress->stars, SZJ_LESSON_COUNT);
    offset += SZJ_LESSON_COUNT;
    memcpy(out + offset, progress->wrong, SZJ_WRONG_BYTES);
    offset += SZJ_WRONG_BYTES;
    out[offset] = blob_checksum(out, offset);
    return offset + 1;
}

bool szj_progress_deserialize(szj_progress_t *out, const uint8_t *data, size_t length) {
    if (!out || !data) return false;
    if (length != SZJ_PROGRESS_BLOB_SIZE) return false;
    if (data[0] != SZJ_PROGRESS_MAGIC) return false;
    if (data[1] != SZJ_PROGRESS_VERSION) return false;
    if (data[length - 1] != blob_checksum(data, length - 1)) return false;

    szj_progress_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    parsed.card_line = (uint16_t)(data[2] | ((uint16_t)data[3] << 8));
    memcpy(parsed.stars, data + SZJ_PROGRESS_HEADER_BYTES, SZJ_LESSON_COUNT);
    memcpy(parsed.wrong, data + SZJ_PROGRESS_HEADER_BYTES + SZJ_LESSON_COUNT, SZJ_WRONG_BYTES);

    // 拒绝越界星级:损坏的数据不能被当成合法进度。
    for (int i = 0; i < SZJ_LESSON_COUNT; i++) {
        if (parsed.stars[i] > SZJ_MAX_STARS_PER_LESSON) return false;
    }
    if (parsed.card_line >= SZJ_LINE_COUNT) return false;

    // 清除位图尾部的保留位,避免它们影响「错题数」的判定。
    const int used_bits = SZJ_LINE_COUNT % 8;
    if (used_bits != 0) {
        parsed.wrong[SZJ_WRONG_BYTES - 1] &= (uint8_t)((1u << used_bits) - 1u);
    }

    *out = parsed;
    return true;
}
