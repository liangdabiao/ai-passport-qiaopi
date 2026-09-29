// tests/test_qpq_progress.c —— 存档的往返与**拒绝路径**。
//
// 拒绝路径是这类代码真正会出问题的地方：NVS 掉电写坏、固件降级、结构体改了字段，
// 都会喂进一段「长度对但内容不对」的字节。所以每个字段都单独测一遍被改坏时是否
// 被拒绝，并断言失败时**没有**污染调用方的结构体 —— 半截状态比读不到更危险。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qpq_progress.h"
#include "qpq_session.h"
#include "qpq_text.h"

static qpq_progress_t s_progress;
static qpq_progress_t s_other;
static uint8_t s_blob[QPQ_PROGRESS_BLOB_SIZE + 8];

static void test_reset_is_a_valid_empty_record(void)
{
    memset(&s_progress, 0xFF, sizeof(s_progress));
    qpq_progress_reset(&s_progress);
    assert(s_progress.runs == 0);
    assert(s_progress.best_score == 0);
    assert(s_progress.best_correct == 0);
    assert(s_progress.best_streak == 0);
    assert(qpq_progress_seen_count(&s_progress) == 0);
    for (uint16_t i = 0; i < (uint16_t)QPQ_QUESTION_COUNT; i++) {
        assert(!qpq_progress_seen(&s_progress, i));
    }
    puts("ok  空档是合法的零值");
}

static void test_seen_bitmap_bounds(void)
{
    qpq_progress_reset(&s_progress);

    qpq_progress_mark_seen(&s_progress, 0);
    qpq_progress_mark_seen(&s_progress, 7);
    qpq_progress_mark_seen(&s_progress, 8);
    qpq_progress_mark_seen(&s_progress, (uint16_t)(QPQ_QUESTION_COUNT - 1));
    assert(qpq_progress_seen_count(&s_progress) == 4);
    assert(qpq_progress_seen(&s_progress, 0));
    assert(!qpq_progress_seen(&s_progress, 1));
    assert(qpq_progress_seen(&s_progress, 7));
    assert(qpq_progress_seen(&s_progress, 8));

    // 越界写不进去，也不会篡改别的位。
    qpq_progress_mark_seen(&s_progress, (uint16_t)QPQ_QUESTION_COUNT);
    qpq_progress_mark_seen(&s_progress, 60000);
    assert(qpq_progress_seen_count(&s_progress) == 4);
    assert(!qpq_progress_seen(&s_progress, (uint16_t)QPQ_QUESTION_COUNT));
    assert(!qpq_progress_seen(&s_progress, 60000));

    // 重复标记是幂等的。
    qpq_progress_mark_seen(&s_progress, 0);
    assert(qpq_progress_seen_count(&s_progress) == 4);
    puts("ok  已见位图边界与幂等");
}

static void test_serialize_round_trip(void)
{
    qpq_progress_reset(&s_progress);
    s_progress.runs = 17;
    s_progress.best_score = 231;
    s_progress.best_correct = 19;
    s_progress.best_streak = 11;
    qpq_progress_mark_seen(&s_progress, 3);
    qpq_progress_mark_seen(&s_progress, 64);

    const uint32_t written =
        qpq_progress_serialize(&s_progress, s_blob, sizeof(s_blob));
    assert(written == QPQ_PROGRESS_BLOB_SIZE);

    memset(&s_other, 0, sizeof(s_other));
    assert(qpq_progress_deserialize(&s_other, s_blob, written) == QPQ_PROGRESS_OK);
    assert(s_other.runs == 17);
    assert(s_other.best_score == 231);
    assert(s_other.best_correct == 19);
    assert(s_other.best_streak == 11);
    assert(qpq_progress_seen(&s_other, 3));
    assert(qpq_progress_seen(&s_other, 64));
    assert(qpq_progress_seen_count(&s_other) == 2);

    // 序列化是确定的：同样的内容产出同样的字节。
    uint8_t again[QPQ_PROGRESS_BLOB_SIZE];
    assert(qpq_progress_serialize(&s_progress, again, sizeof(again)) == written);
    assert(memcmp(again, s_blob, written) == 0);
    puts("ok  存档往返一致且序列化确定");
}

static void test_serialize_capacity_is_all_or_nothing(void)
{
    qpq_progress_reset(&s_progress);
    // 容量不足：返回 0，不写半截数据。
    assert(qpq_progress_serialize(&s_progress, s_blob, QPQ_PROGRESS_BLOB_SIZE - 1) == 0);
    assert(qpq_progress_serialize(&s_progress, NULL, sizeof(s_blob)) == 0);
    assert(qpq_progress_serialize(NULL, s_blob, sizeof(s_blob)) == 0);
    puts("ok  序列化容量不足整体失败");
}

static void test_rejection_paths_do_not_touch_target(void)
{
    qpq_progress_reset(&s_progress);
    s_progress.runs = 5;
    s_progress.best_score = 90;
    const uint32_t written =
        qpq_progress_serialize(&s_progress, s_blob, sizeof(s_blob));
    assert(written == QPQ_PROGRESS_BLOB_SIZE);

    // 目标结构体预置成可辨认的值：任何失败都不该改动它。
    const struct {
        uint32_t runs;
        int32_t best_score;
    } sentinel = {0xDEADBEEFu, -777};

    struct {
        qpq_progress_status_t expected;
        const char *label;
    } cases[6];
    uint32_t count = 0;

    // 长度不对（短一个字节 / 多一个字节）。
    uint8_t wrong_length[QPQ_PROGRESS_BLOB_SIZE + 1];
    memcpy(wrong_length, s_blob, written);
    wrong_length[written] = 0;
    cases[count].expected = QPQ_PROGRESS_ERR_LENGTH;
    cases[count].label = "长度多一字节";
    count++;

    // 魔数不符：把第一个字节改掉会同时破坏校验和，所以魔数检查必须排在前面。
    uint8_t bad_magic[QPQ_PROGRESS_BLOB_SIZE];
    memcpy(bad_magic, s_blob, written);
    bad_magic[0] = (uint8_t)(bad_magic[0] ^ 0xFFu);
    cases[count].expected = QPQ_PROGRESS_ERR_MAGIC;
    cases[count].label = "魔数不符";
    count++;

    // 版本不符。
    uint8_t bad_version[QPQ_PROGRESS_BLOB_SIZE];
    memcpy(bad_version, s_blob, written);
    bad_version[4] = (uint8_t)(bad_version[4] + 1u);
    cases[count].expected = QPQ_PROGRESS_ERR_VERSION;
    cases[count].label = "版本不符";
    count++;

    // 正文里翻一个位：长度、魔数、版本都对，只有校验和能抓出来。
    uint8_t bad_body[QPQ_PROGRESS_BLOB_SIZE];
    memcpy(bad_body, s_blob, written);
    bad_body[10] = (uint8_t)(bad_body[10] ^ 0x01u);
    cases[count].expected = QPQ_PROGRESS_ERR_CHECKSUM;
    cases[count].label = "正文翻一位";
    count++;

    // 已见位图里翻一位：同样只有校验和能抓。
    uint8_t bad_bitmap[QPQ_PROGRESS_BLOB_SIZE];
    memcpy(bad_bitmap, s_blob, written);
    bad_bitmap[QPQ_PROGRESS_BLOB_SIZE - 5] = (uint8_t)(bad_bitmap[QPQ_PROGRESS_BLOB_SIZE - 5] ^ 0x80u);
    cases[count].expected = QPQ_PROGRESS_ERR_CHECKSUM;
    cases[count].label = "位图翻一位";
    count++;

    // 校验和本身被改坏。
    uint8_t bad_sum[QPQ_PROGRESS_BLOB_SIZE];
    memcpy(bad_sum, s_blob, written);
    bad_sum[QPQ_PROGRESS_BLOB_SIZE - 1] = (uint8_t)(bad_sum[QPQ_PROGRESS_BLOB_SIZE - 1] ^ 0x40u);
    cases[count].expected = QPQ_PROGRESS_ERR_CHECKSUM;
    cases[count].label = "校验和被改";
    count++;

    for (uint32_t i = 0; i < 4; i++) {
        memset(&s_other, 0xEE, sizeof(s_other));
        const uint8_t *payload = (i == 0) ? wrong_length
                               : (i == 1) ? bad_magic
                               : (i == 2) ? bad_version
                                          : bad_body;
        const uint32_t length = (i == 0) ? written + 1 : written;
        const qpq_progress_status_t status =
            qpq_progress_deserialize(&s_other, payload, length);
        assert(status == cases[i].expected);
        // 失败不得污染目标：这里仍然是预置值。
        assert(s_other.runs == 0xEEEEEEEEu);
        assert(s_other.best_score == (int32_t)0xEEEEEEEEu);
    }
    for (uint32_t i = 4; i < count; i++) {
        memset(&s_other, 0xEE, sizeof(s_other));
        const uint8_t *payload = (i == 4) ? bad_bitmap : bad_sum;
        assert(qpq_progress_deserialize(&s_other, payload, written) ==
               cases[i].expected);
        assert(s_other.runs == 0xEEEEEEEEu);
    }

    // 空指针与长度为零。
    assert(qpq_progress_deserialize(NULL, s_blob, written) == QPQ_PROGRESS_ERR_NULL);
    assert(qpq_progress_deserialize(&s_other, NULL, written) == QPQ_PROGRESS_ERR_NULL);
    assert(qpq_progress_deserialize(&s_other, s_blob, 0) == QPQ_PROGRESS_ERR_LENGTH);

    (void)sentinel;
    puts("ok  六条拒绝路径（长度/魔数/版本/正文/位图/校验和）均不污染目标");
}

static void test_record_run_updates_bests_and_seen(void)
{
    qpq_progress_reset(&s_progress);

    qpq_session_t session;
    qpq_session_init(&session, 4242);
    qpq_session_start(&session);
    for (uint16_t i = 0; i < (uint16_t)QPQ_RUN_LENGTH; i++) {
        qpq_session_question(&session);
        const qpq_question_t *question = qpq_session_question(&session);
        // 交替答对答错，得到非满分的成绩。
        while (session.cursor != (uint8_t)((question->answer + (i & 1u)) %
                                           (uint8_t)QPQ_OPTION_COUNT)) {
            qpq_session_key(&session, QPQ_KEY_DOWN);
        }
        qpq_session_key(&session, QPQ_KEY_OK);
        qpq_session_key(&session, QPQ_KEY_OK);
    }
    assert(session.stage == QPQ_STAGE_SUMMARY);

    qpq_progress_record_run(&s_progress, &session);
    assert(s_progress.runs == 1);
    assert(s_progress.best_score == session.score);
    assert(s_progress.best_correct == session.correct_count);
    assert(s_progress.best_streak == session.max_streak);
    assert(qpq_progress_seen_count(&s_progress) == (uint16_t)QPQ_RUN_LENGTH);
    for (uint16_t i = 0; i < (uint16_t)QPQ_RUN_LENGTH; i++) {
        assert(qpq_progress_seen(&s_progress, session.order[i]));
    }

    // 再记一局更差的成绩：局数增加，最好成绩不变。
    const int32_t previous_best = s_progress.best_score;
    const uint16_t previous_correct = s_progress.best_correct;
    const uint16_t previous_streak = s_progress.best_streak;

    qpq_session_t weaker;
    qpq_session_init(&weaker, 99);
    qpq_session_start(&weaker);
    for (uint16_t i = 0; i < (uint16_t)QPQ_RUN_LENGTH; i++) {
        const qpq_question_t *question = qpq_session_question(&weaker);
        while (weaker.cursor != (uint8_t)((question->answer + 1u) %
                                          (uint8_t)QPQ_OPTION_COUNT)) {
            qpq_session_key(&weaker, QPQ_KEY_DOWN);
        }
        qpq_session_key(&weaker, QPQ_KEY_OK);
        qpq_session_key(&weaker, QPQ_KEY_OK);
    }
    qpq_progress_record_run(&s_progress, &weaker);
    assert(s_progress.runs == 2);
    assert(s_progress.best_score == previous_best);
    assert(s_progress.best_correct == previous_correct);
    assert(s_progress.best_streak == previous_streak);

    // 未开始的局不该被记进去。
    qpq_session_t untouched;
    qpq_session_init(&untouched, 1);
    qpq_progress_record_run(&s_progress, &untouched);
    assert(s_progress.runs == 2);

    qpq_progress_record_run(NULL, &session);
    qpq_progress_record_run(&s_progress, NULL);
    puts("ok  记一局：局数递增、最好成绩只升不降、本局题目标为已见");
}

static void test_blob_size_is_exactly_as_documented(void)
{
    // 常量与序列化出来的长度必须一致，否则 NVS 写入会截断自己。
    assert(QPQ_PROGRESS_BLOB_SIZE == 24u + QPQ_SEEN_BYTES);
    assert(QPQ_SEEN_BYTES == (QPQ_QUESTION_COUNT + 7) / 8);
    qpq_progress_reset(&s_progress);
    assert(qpq_progress_serialize(&s_progress, s_blob, sizeof(s_blob)) ==
           QPQ_PROGRESS_BLOB_SIZE);
    printf("ok  存档长度常量自洽（%u 字节：24 + %u 字节位图）\n",
           (unsigned)QPQ_PROGRESS_BLOB_SIZE, (unsigned)QPQ_SEEN_BYTES);
}

int main(void)
{
    test_reset_is_a_valid_empty_record();
    test_seen_bitmap_bounds();
    test_serialize_round_trip();
    test_serialize_capacity_is_all_or_nothing();
    test_rejection_paths_do_not_touch_target();
    test_record_run_updates_bests_and_seen();
    test_blob_size_is_exactly_as_documented();
    puts("test_qpq_progress: PASS");
    return 0;
}
