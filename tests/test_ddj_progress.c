// tests/test_ddj_progress.c —— 进度模型、位图边界与序列化往返。
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ddj_progress.h"
#include "ddj_text.h"

// 与 ddj_progress.c 里同一套算法，这里独立重写一份 —— 直接复用被测代码的
// 校验和函数，等于没验。
static uint8_t checksum(const uint8_t *data, size_t length)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) sum += data[i];
    return (uint8_t)(sum ^ 0xA5u);
}

int main(void)
{
    assert(DDJ_NOTES_BYTES == 81);
    assert(DDJ_BITMAP_BYTES == 11);
    assert(DDJ_PROGRESS_HEADER_BYTES == 4);
    assert(DDJ_PROGRESS_BLOB_SIZE == 4 + 81 + 11 + 11 + 1);
    assert(DDJ_PROGRESS_BLOB_SIZE == 108);

    ddj_progress_t p;
    ddj_progress_reset(&p);
    assert(p.sessions == 0);
    assert(ddj_progress_sessions(&p) == 0);
    assert(ddj_progress_read_count(&p) == 0);
    assert(ddj_progress_starred_count(&p) == 0);
    assert(ddj_progress_slot(&p, 0) == DDJ_SLOT_NONE);
    assert(!ddj_progress_has_read(&p, 0));
    assert(!ddj_progress_is_starred(&p, 0));
    assert(ddj_progress_next_chapter(&p, 1) == 0);

    // 表态：首尾两章都能记，越界槽位被拒绝。
    ddj_progress_set_slot(&p, 0, DDJ_SLOT_LANDED);
    ddj_progress_set_slot(&p, DDJ_TOTAL_CHAPTERS - 1, DDJ_SLOT_MISSED);
    assert(ddj_progress_slot(&p, 0) == DDJ_SLOT_LANDED);
    assert(ddj_progress_slot(&p, DDJ_TOTAL_CHAPTERS - 1) == DDJ_SLOT_MISSED);
    assert(ddj_progress_slot(&p, 1) == DDJ_SLOT_NONE);

    ddj_progress_set_slot(&p, 1, DDJ_SLOT_COUNT);
    assert(ddj_progress_slot(&p, 1) == DDJ_SLOT_NONE);
    ddj_progress_set_slot(&p, 1, (ddj_slot_t)-1);
    assert(ddj_progress_slot(&p, 1) == DDJ_SLOT_NONE);
    ddj_progress_set_slot(&p, -1, DDJ_SLOT_LANDED);
    ddj_progress_set_slot(&p, DDJ_TOTAL_CHAPTERS, DDJ_SLOT_LANDED);
    assert(ddj_progress_slot(&p, -1) == DDJ_SLOT_NONE);
    assert(ddj_progress_slot(&p, DDJ_TOTAL_CHAPTERS) == DDJ_SLOT_NONE);

    // 位图跨字节边界：第 7/8 位与最后一位（第 81 位落在第 11 字节的 bit0）。
    ddj_progress_mark_read(&p, 7);
    ddj_progress_mark_read(&p, 8);
    ddj_progress_mark_read(&p, DDJ_TOTAL_CHAPTERS - 1);
    assert(ddj_progress_has_read(&p, 7));
    assert(ddj_progress_has_read(&p, 8));
    assert(ddj_progress_has_read(&p, DDJ_TOTAL_CHAPTERS - 1));
    assert(!ddj_progress_has_read(&p, 9));
    assert(ddj_progress_read_count(&p) == 3);

    ddj_progress_set_starred(&p, 11, true);
    assert(ddj_progress_is_starred(&p, 11));
    assert(ddj_progress_starred_count(&p) == 1);
    ddj_progress_set_starred(&p, 11, false);
    assert(!ddj_progress_is_starred(&p, 11));
    assert(ddj_progress_starred_count(&p) == 0);

    // 越界位不写坏内存。
    ddj_progress_mark_read(&p, -1);
    ddj_progress_mark_read(&p, DDJ_TOTAL_CHAPTERS);
    assert(ddj_progress_read_count(&p) == 3);

    // 日课次数只增，且到顶不再溢出回头。
    ddj_progress_end_session(&p);
    assert(ddj_progress_sessions(&p) == 1);
    p.sessions = UINT16_MAX;
    ddj_progress_end_session(&p);
    assert(p.sessions == UINT16_MAX);

    // 下一章：没读过的优先，全读过就回到第 1 章。
    ddj_progress_t walk;
    ddj_progress_reset(&walk);
    assert(ddj_progress_next_chapter(&walk, 3) == 0);
    ddj_progress_mark_read(&walk, 0);
    assert(ddj_progress_next_chapter(&walk, 3) == 1);
    ddj_progress_mark_read(&walk, 1);
    assert(ddj_progress_next_chapter(&walk, 3) == 2);
    ddj_progress_mark_read(&walk, 2);
    assert(ddj_progress_next_chapter(&walk, 3) == 0);
    assert(ddj_progress_next_chapter(&walk, 0) == -1);
    // 章数超过全本时按全本夹紧，不越界读位图；于是第 3 章（0 基）就是
    // 第一本没读过的章。
    assert(ddj_progress_next_chapter(&walk, 999) == 3);

    assert(strcmp(ddj_slot_name(DDJ_SLOT_LANDED), "接了") == 0);
    assert(strcmp(ddj_slot_name(DDJ_SLOT_CHEWING), "还要想") == 0);
    assert(strcmp(ddj_slot_name(DDJ_SLOT_MISSED), "没接上") == 0);
    assert(strcmp(ddj_slot_name(DDJ_SLOT_NONE), "未表态") == 0);
    assert(strcmp(ddj_slot_name(99), "未表态") == 0);

    // NULL 容错。
    assert(ddj_progress_read_count(NULL) == 0);
    assert(ddj_progress_starred_count(NULL) == 0);
    assert(ddj_progress_sessions(NULL) == 0);
    assert(ddj_progress_slot(NULL, 0) == DDJ_SLOT_NONE);
    assert(!ddj_progress_has_read(NULL, 0));
    assert(!ddj_progress_is_starred(NULL, 0));
    assert(ddj_progress_next_chapter(NULL, 1) == 0);
    assert(!ddj_progress_index_valid(-1));
    assert(!ddj_progress_index_valid(DDJ_TOTAL_CHAPTERS));
    assert(ddj_progress_index_valid(0));
    assert(ddj_progress_index_valid(DDJ_TOTAL_CHAPTERS - 1));

    ddj_progress_set_slot(NULL, 0, DDJ_SLOT_LANDED);
    ddj_progress_mark_read(NULL, 0);
    ddj_progress_set_starred(NULL, 0, true);
    ddj_progress_end_session(NULL);
    ddj_progress_reset(NULL);

    // ---- 序列化往返 ----
    ddj_progress_t q;
    ddj_progress_reset(&q);
    q.sessions = 42;
    ddj_progress_set_slot(&q, 0, DDJ_SLOT_CHEWING);
    ddj_progress_set_slot(&q, 80, DDJ_SLOT_MISSED);
    ddj_progress_mark_read(&q, 0);
    ddj_progress_mark_read(&q, 80);
    ddj_progress_set_starred(&q, 5, true);

    uint8_t blob[DDJ_PROGRESS_BLOB_SIZE];
    memset(blob, 0, sizeof(blob));
    const size_t size = ddj_progress_serialize(&q, blob, sizeof(blob));
    assert(size == DDJ_PROGRESS_BLOB_SIZE);
    assert(blob[0] == DDJ_PROGRESS_MAGIC);
    assert(blob[1] == DDJ_PROGRESS_VERSION);
    // 日课次数按小端存。
    assert(blob[2] == 42 && blob[3] == 0);

    ddj_progress_t back;
    assert(ddj_progress_deserialize(&back, blob, size));
    assert(back.sessions == 42);
    assert(ddj_progress_slot(&back, 0) == DDJ_SLOT_CHEWING);
    assert(ddj_progress_slot(&back, 80) == DDJ_SLOT_MISSED);
    assert(ddj_progress_has_read(&back, 0));
    assert(ddj_progress_has_read(&back, 80));
    assert(ddj_progress_read_count(&back) == 2);
    assert(ddj_progress_is_starred(&back, 5));
    assert(ddj_progress_starred_count(&back) == 1);

    // 容量不足与空指针。
    assert(ddj_progress_serialize(&q, blob, sizeof(blob) - 1) == 0);
    assert(ddj_progress_serialize(&q, NULL, sizeof(blob)) == 0);
    assert(ddj_progress_serialize(NULL, blob, sizeof(blob)) == 0);

    uint8_t bad[DDJ_PROGRESS_BLOB_SIZE];

    // 长度不对。
    memcpy(bad, blob, sizeof(bad));
    assert(!ddj_progress_deserialize(&back, bad, sizeof(bad) - 1));
    assert(!ddj_progress_deserialize(&back, bad, sizeof(bad) + 1));

    // magic 不对。
    memcpy(bad, blob, sizeof(bad));
    bad[0] = 0x00;
    assert(!ddj_progress_deserialize(&back, bad, sizeof(bad)));

    // 版本不对。
    memcpy(bad, blob, sizeof(bad));
    bad[1] = DDJ_PROGRESS_VERSION + 1;
    assert(!ddj_progress_deserialize(&back, bad, sizeof(bad)));

    // 校验和不对（数据被改坏）。
    memcpy(bad, blob, sizeof(bad));
    bad[DDJ_PROGRESS_BLOB_SIZE - 1] ^= 0xFF;
    assert(!ddj_progress_deserialize(&back, bad, sizeof(bad)));

    // 表态值越界：校验和正确也必须拒绝。
    memcpy(bad, blob, sizeof(bad));
    bad[DDJ_PROGRESS_HEADER_BYTES + 3] = DDJ_SLOT_COUNT;
    bad[DDJ_PROGRESS_BLOB_SIZE - 1] = checksum(bad, DDJ_PROGRESS_BLOB_SIZE - 1);
    assert(!ddj_progress_deserialize(&back, bad, sizeof(bad)));

    // 拒绝时不得改动输出。
    ddj_progress_t untouched;
    ddj_progress_reset(&untouched);
    untouched.sessions = 777;
    memcpy(bad, blob, sizeof(bad));
    bad[0] = 0x00;
    assert(!ddj_progress_deserialize(&untouched, bad, sizeof(bad)));
    assert(untouched.sessions == 777);

    // 位图尾部用不到的保留位要被清掉，免得算进「读过几章」。
    memcpy(bad, blob, sizeof(bad));
    bad[DDJ_PROGRESS_HEADER_BYTES + DDJ_NOTES_BYTES + DDJ_BITMAP_BYTES - 1] |= 0xFEu;
    bad[DDJ_PROGRESS_BLOB_SIZE - 1] = checksum(bad, DDJ_PROGRESS_BLOB_SIZE - 1);
    assert(ddj_progress_deserialize(&back, bad, sizeof(bad)));
    assert(ddj_progress_read_count(&back) == 2);

    assert(!ddj_progress_deserialize(NULL, blob, sizeof(blob)));
    assert(!ddj_progress_deserialize(&back, NULL, sizeof(blob)));

    // reset 必须清干净。
    ddj_progress_reset(&q);
    assert(q.sessions == 0);
    assert(ddj_progress_read_count(&q) == 0);
    assert(ddj_progress_starred_count(&q) == 0);
    assert(ddj_progress_slot(&q, 0) == DDJ_SLOT_NONE);
    for (int i = 0; i < DDJ_NOTES_BYTES; i++) assert(q.notes[i] == 0);
    return 0;
}
