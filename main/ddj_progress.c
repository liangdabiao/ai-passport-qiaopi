// main/ddj_progress.c —— 见 ddj_progress.h。
#include "ddj_progress.h"

#include <string.h>

static uint8_t blob_checksum(const uint8_t *data, size_t length)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) sum += data[i];
    return (uint8_t)(sum ^ 0xA5u);
}

static bool bitmap_get(const uint8_t *bitmap, int index)
{
    return (bitmap[index / 8] & (uint8_t)(1u << (index % 8))) != 0;
}

static void bitmap_set(uint8_t *bitmap, int index, bool value)
{
    const uint8_t mask = (uint8_t)(1u << (index % 8));
    if (value) {
        bitmap[index / 8] |= mask;
    } else {
        bitmap[index / 8] &= (uint8_t)~mask;
    }
}

bool ddj_progress_index_valid(int index)
{
    return index >= 0 && index < DDJ_TOTAL_CHAPTERS;
}

void ddj_progress_reset(ddj_progress_t *progress)
{
    if (!progress) return;
    memset(progress, 0, sizeof(*progress));
}

ddj_slot_t ddj_progress_slot(const ddj_progress_t *progress, int index)
{
    if (!progress || !ddj_progress_index_valid(index)) return DDJ_SLOT_NONE;
    const uint8_t value = progress->notes[index];
    return value < DDJ_SLOT_COUNT ? (ddj_slot_t)value : DDJ_SLOT_NONE;
}

void ddj_progress_set_slot(ddj_progress_t *progress, int index, ddj_slot_t slot)
{
    if (!progress || !ddj_progress_index_valid(index)) return;
    if (slot < DDJ_SLOT_NONE || slot >= DDJ_SLOT_COUNT) return;
    progress->notes[index] = (uint8_t)slot;
}

bool ddj_progress_has_read(const ddj_progress_t *progress, int index)
{
    if (!progress || !ddj_progress_index_valid(index)) return false;
    return bitmap_get(progress->read, index);
}

void ddj_progress_mark_read(ddj_progress_t *progress, int index)
{
    if (!progress || !ddj_progress_index_valid(index)) return;
    bitmap_set(progress->read, index, true);
}

bool ddj_progress_is_starred(const ddj_progress_t *progress, int index)
{
    if (!progress || !ddj_progress_index_valid(index)) return false;
    return bitmap_get(progress->starred, index);
}

void ddj_progress_set_starred(ddj_progress_t *progress, int index, bool starred)
{
    if (!progress || !ddj_progress_index_valid(index)) return;
    bitmap_set(progress->starred, index, starred);
}

int ddj_progress_read_count(const ddj_progress_t *progress)
{
    if (!progress) return 0;
    int total = 0;
    for (int i = 0; i < DDJ_TOTAL_CHAPTERS; i++) {
        if (bitmap_get(progress->read, i)) total++;
    }
    return total;
}

int ddj_progress_starred_count(const ddj_progress_t *progress)
{
    if (!progress) return 0;
    int total = 0;
    for (int i = 0; i < DDJ_TOTAL_CHAPTERS; i++) {
        if (bitmap_get(progress->starred, i)) total++;
    }
    return total;
}

int ddj_progress_sessions(const ddj_progress_t *progress)
{
    return progress ? (int)progress->sessions : 0;
}

void ddj_progress_end_session(ddj_progress_t *progress)
{
    if (!progress) return;
    if (progress->sessions < UINT16_MAX) progress->sessions++;
}

int ddj_progress_next_chapter(const ddj_progress_t *progress, int chapter_count)
{
    if (chapter_count <= 0) return -1;
    if (chapter_count > DDJ_TOTAL_CHAPTERS) chapter_count = DDJ_TOTAL_CHAPTERS;
    for (int i = 0; i < chapter_count; i++) {
        if (!ddj_progress_has_read(progress, i)) return i;
    }
    // 都读过了，就从第一章重新走 —— 这个应用本来就是反复读的。
    return 0;
}

const char *ddj_slot_name(int slot)
{
    switch (slot) {
        case DDJ_SLOT_LANDED:  return "接了";
        case DDJ_SLOT_CHEWING: return "还要想";
        case DDJ_SLOT_MISSED:  return "没接上";
        default:               return "未表态";
    }
}

size_t ddj_progress_serialize(const ddj_progress_t *progress, uint8_t *out, size_t capacity)
{
    if (!progress || !out) return 0;
    if (capacity < DDJ_PROGRESS_BLOB_SIZE) return 0;

    size_t offset = 0;
    out[offset++] = DDJ_PROGRESS_MAGIC;
    out[offset++] = DDJ_PROGRESS_VERSION;
    out[offset++] = (uint8_t)(progress->sessions & 0xFFu);
    out[offset++] = (uint8_t)((progress->sessions >> 8) & 0xFFu);
    memcpy(out + offset, progress->notes, DDJ_NOTES_BYTES);
    offset += DDJ_NOTES_BYTES;
    memcpy(out + offset, progress->read, DDJ_BITMAP_BYTES);
    offset += DDJ_BITMAP_BYTES;
    memcpy(out + offset, progress->starred, DDJ_BITMAP_BYTES);
    offset += DDJ_BITMAP_BYTES;
    out[offset] = blob_checksum(out, offset);
    return offset + 1;
}

bool ddj_progress_deserialize(ddj_progress_t *out, const uint8_t *data, size_t length)
{
    if (!out || !data) return false;
    if (length != DDJ_PROGRESS_BLOB_SIZE) return false;
    if (data[0] != DDJ_PROGRESS_MAGIC) return false;
    if (data[1] != DDJ_PROGRESS_VERSION) return false;
    if (data[length - 1] != blob_checksum(data, length - 1)) return false;

    ddj_progress_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    parsed.sessions = (uint16_t)(data[2] | ((uint16_t)data[3] << 8));
    memcpy(parsed.notes, data + DDJ_PROGRESS_HEADER_BYTES, DDJ_NOTES_BYTES);
    memcpy(parsed.read, data + DDJ_PROGRESS_HEADER_BYTES + DDJ_NOTES_BYTES, DDJ_BITMAP_BYTES);
    memcpy(parsed.starred,
           data + DDJ_PROGRESS_HEADER_BYTES + DDJ_NOTES_BYTES + DDJ_BITMAP_BYTES,
           DDJ_BITMAP_BYTES);

    // 越界的表态值说明存档被改坏了，不能当成合法进度。
    for (int i = 0; i < DDJ_NOTES_BYTES; i++) {
        if (parsed.notes[i] >= DDJ_SLOT_COUNT) return false;
    }

    // 清掉位图尾部用不到的保留位，免得它们影响「读过几章」的计数。
    const int used_bits = DDJ_TOTAL_CHAPTERS % 8;
    if (used_bits != 0) {
        const uint8_t keep = (uint8_t)((1u << used_bits) - 1u);
        parsed.read[DDJ_BITMAP_BYTES - 1] &= keep;
        parsed.starred[DDJ_BITMAP_BYTES - 1] &= keep;
    }

    *out = parsed;
    return true;
}
