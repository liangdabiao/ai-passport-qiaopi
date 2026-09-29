// tests/test_szj_progress.c —— 进度的统计、错题位图与序列化往返。
#include <assert.h>
#include <string.h>

#include "szj_progress.h"
#include "szj_text.h"

// 与 szj_progress.c 中的算法一致。这里独立重写一份,才能真的验证校验和逻辑。
static uint8_t checksum(const uint8_t *data, size_t length)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) sum += data[i];
    return (uint8_t)(sum ^ 0xA5u);
}

int main(void)
{
    szj_progress_t p;
    szj_progress_reset(&p);

    assert(SZJ_LESSON_COUNT == SZJ_STANZA_COUNT);
    assert(SZJ_MAX_STARS_PER_LESSON == 3);
    assert(SZJ_WRONG_BYTES == (SZJ_LINE_COUNT + 7) / 8);
    assert(SZJ_PROGRESS_BLOB_SIZE ==
           SZJ_PROGRESS_HEADER_BYTES + SZJ_LESSON_COUNT + SZJ_WRONG_BYTES + 1);

    assert(szj_progress_total_stars(&p) == 0);
    assert(szj_progress_completed_lessons(&p) == 0);
    assert(szj_progress_perfect_lessons(&p) == 0);
    assert(szj_progress_next_lesson(&p) == 0);
    assert(p.card_line == 0);

    // 记星:只增不减,重练不会把已有星星降下来。
    szj_progress_record_lesson(&p, 0, 2);
    assert(p.stars[0] == 2);
    szj_progress_record_lesson(&p, 0, 1);
    assert(p.stars[0] == 2);
    szj_progress_record_lesson(&p, 0, 3);
    assert(p.stars[0] == 3);
    assert(szj_progress_total_stars(&p) == 3);
    assert(szj_progress_completed_lessons(&p) == 1);
    assert(szj_progress_perfect_lessons(&p) == 1);
    assert(szj_progress_next_lesson(&p) == 1);

    // 越界课号被忽略,超范围星级被夹紧,负数当作 0。
    szj_progress_record_lesson(&p, -1, 3);
    szj_progress_record_lesson(&p, SZJ_LESSON_COUNT, 3);
    assert(szj_progress_total_stars(&p) == 3);
    szj_progress_record_lesson(&p, 5, 99);
    assert(p.stars[5] == SZJ_MAX_STARS_PER_LESSON);
    szj_progress_record_lesson(&p, 6, -1);
    assert(p.stars[6] == 0);

    // NULL 容错。
    assert(szj_progress_total_stars(NULL) == 0);
    assert(szj_progress_completed_lessons(NULL) == 0);
    assert(szj_progress_perfect_lessons(NULL) == 0);
    assert(szj_progress_next_lesson(NULL) == 0);
    assert(szj_progress_wrong_count(NULL) == 0);
    assert(!szj_progress_is_wrong(NULL, 0));
    szj_progress_record_lesson(NULL, 0, 3);
    szj_progress_mark_wrong(NULL, 0);

    // 全部通关后 next_lesson 回到第 0 课复习。
    szj_progress_t full;
    szj_progress_reset(&full);
    for (int i = 0; i < SZJ_LESSON_COUNT; i++) szj_progress_record_lesson(&full, i, 1);
    assert(szj_progress_completed_lessons(&full) == SZJ_LESSON_COUNT);
    assert(szj_progress_next_lesson(&full) == 0);

    // 错题位图:跨字节边界(第 7/8 位)与最后一句都要正确。
    assert(!szj_progress_is_wrong(&p, 3));
    szj_progress_mark_wrong(&p, 3);
    szj_progress_mark_wrong(&p, 7);
    szj_progress_mark_wrong(&p, 8);
    szj_progress_mark_wrong(&p, SZJ_LINE_COUNT - 1);
    assert(szj_progress_is_wrong(&p, 3));
    assert(szj_progress_is_wrong(&p, 7));
    assert(szj_progress_is_wrong(&p, 8));
    assert(szj_progress_is_wrong(&p, SZJ_LINE_COUNT - 1));
    assert(!szj_progress_is_wrong(&p, 4));
    assert(szj_progress_wrong_count(&p) == 4);

    // 越界位不写坏内存。
    szj_progress_mark_wrong(&p, SZJ_LINE_COUNT);
    szj_progress_mark_wrong(&p, -1);
    assert(szj_progress_wrong_count(&p) == 4);
    assert(!szj_progress_is_wrong(&p, -1));
    assert(!szj_progress_is_wrong(&p, SZJ_LINE_COUNT));

    // 按经文顺序导出。
    int list[8];
    assert(szj_progress_wrong_list(&p, list, 8) == 4);
    assert(list[0] == 3 && list[1] == 7 && list[2] == 8 && list[3] == SZJ_LINE_COUNT - 1);
    assert(szj_progress_wrong_list(&p, list, 2) == 2);
    assert(szj_progress_wrong_list(&p, list, 0) == 0);
    assert(szj_progress_wrong_list(NULL, list, 8) == 0);

    szj_progress_clear_wrong(&p, 3);
    assert(!szj_progress_is_wrong(&p, 3));
    assert(szj_progress_wrong_count(&p) == 3);

    // 序列化往返。
    szj_progress_t q;
    szj_progress_reset(&q);
    szj_progress_record_lesson(&q, 7, 2);
    szj_progress_mark_wrong(&q, 11);
    q.card_line = 42;

    uint8_t blob[SZJ_PROGRESS_BLOB_SIZE];
    memset(blob, 0, sizeof(blob));
    const size_t size = szj_progress_serialize(&q, blob, sizeof(blob));
    assert(size == SZJ_PROGRESS_BLOB_SIZE);
    assert(blob[0] == SZJ_PROGRESS_MAGIC);
    assert(blob[1] == SZJ_PROGRESS_VERSION);
    assert(blob[2] == 42 && blob[3] == 0);

    szj_progress_t back;
    assert(szj_progress_deserialize(&back, blob, size));
    assert(back.card_line == 42);
    assert(back.stars[7] == 2);
    assert(szj_progress_is_wrong(&back, 11));
    assert(szj_progress_total_stars(&back) == szj_progress_total_stars(&q));
    assert(szj_progress_wrong_count(&back) == 1);

    // 容量不足与空指针。
    assert(szj_progress_serialize(&q, blob, sizeof(blob) - 1) == 0);
    assert(szj_progress_serialize(&q, NULL, sizeof(blob)) == 0);
    assert(szj_progress_serialize(NULL, blob, sizeof(blob)) == 0);

    uint8_t bad[SZJ_PROGRESS_BLOB_SIZE];

    // 长度不对。
    memcpy(bad, blob, sizeof(bad));
    assert(!szj_progress_deserialize(&back, bad, sizeof(bad) - 1));
    assert(!szj_progress_deserialize(&back, bad, sizeof(bad) + 1));

    // magic 不对。
    memcpy(bad, blob, sizeof(bad));
    bad[0] = 0x00;
    assert(!szj_progress_deserialize(&back, bad, sizeof(bad)));

    // 版本不对。
    memcpy(bad, blob, sizeof(bad));
    bad[1] = SZJ_PROGRESS_VERSION + 1;
    assert(!szj_progress_deserialize(&back, bad, sizeof(bad)));

    // 校验和不对(数据被改坏)。
    memcpy(bad, blob, sizeof(bad));
    bad[SZJ_PROGRESS_BLOB_SIZE - 1] ^= 0xFF;
    assert(!szj_progress_deserialize(&back, bad, sizeof(bad)));

    // 星星越界:校验和正确也要拒绝。
    memcpy(bad, blob, sizeof(bad));
    bad[SZJ_PROGRESS_HEADER_BYTES + 0] = SZJ_MAX_STARS_PER_LESSON + 1;
    bad[SZJ_PROGRESS_BLOB_SIZE - 1] = checksum(bad, SZJ_PROGRESS_BLOB_SIZE - 1);
    assert(!szj_progress_deserialize(&back, bad, sizeof(bad)));

    // 认字卡游标越界:同样要拒绝。
    memcpy(bad, blob, sizeof(bad));
    bad[2] = (uint8_t)(SZJ_LINE_COUNT & 0xFF);
    bad[3] = (uint8_t)((SZJ_LINE_COUNT >> 8) & 0xFF);
    bad[SZJ_PROGRESS_BLOB_SIZE - 1] = checksum(bad, SZJ_PROGRESS_BLOB_SIZE - 1);
    assert(!szj_progress_deserialize(&back, bad, sizeof(bad)));

    // 拒绝时不得改动输出。
    szj_progress_t untouched;
    szj_progress_reset(&untouched);
    untouched.card_line = 123;
    memcpy(bad, blob, sizeof(bad));
    bad[0] = 0x00;
    assert(!szj_progress_deserialize(&untouched, bad, sizeof(bad)));
    assert(untouched.card_line == 123);

    assert(!szj_progress_deserialize(NULL, blob, sizeof(blob)));
    assert(!szj_progress_deserialize(&back, NULL, sizeof(blob)));

    // reset 必须清干净。
    szj_progress_reset(&q);
    assert(szj_progress_total_stars(&q) == 0);
    assert(szj_progress_wrong_count(&q) == 0);
    assert(q.card_line == 0);
    return 0;
}
