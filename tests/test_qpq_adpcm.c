// tests/test_qpq_adpcm.c —— ADPCM 解码器。
//
// 这个测试的核心是**跨语言等价**：固定向量由 tools/qiaopi/adpcm.py 的参考实现
// 编码，C 解码器必须解出与参考实现记录的序列完全相同的值。没有这条，「C 与
// Python 一致」就只是句口号；有这条，编码器和解码器任一侧改动都会被抓住。
//
// 另外的断言覆盖流式的性质：分块大小不影响结果、rewind 后可重放、空流与越界
// 请求的行为明确。这些是播放器能按「一次解一小块」工作的前提。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qpq_adpcm.h"
#include "qpq_adpcm_fixture.h"

// 前 48 个样本是衰减正弦（真实信号），后 16 个是满幅方波。方波在 16 kHz 上接近
// 奈奎斯特频率，ADPCM 本来就追不上，所以质量指标只按前 48 个算。
#define FIXTURE_SIGNAL_SAMPLES 48
#define FIXTURE_SIGNAL_MAX_ERROR 600

static int16_t s_bulk[QPQ_ADPCM_FIXTURE_SAMPLES];
static int16_t s_chunked[QPQ_ADPCM_FIXTURE_SAMPLES];
static int16_t s_small[4];

static void test_encoded_size_matches_fixture(void)
{
    assert(qpq_adpcm_encoded_size(QPQ_ADPCM_FIXTURE_SAMPLES) ==
           sizeof(qpq_adpcm_fixture_encoded));
    // 4 字节头 + ceil((N-1)/2)。N=64 时是 4 + 32 = 36。
    assert(qpq_adpcm_encoded_size(64) == 36);
    assert(qpq_adpcm_encoded_size(1) == 4);
    assert(qpq_adpcm_encoded_size(2) == 5);
    assert(qpq_adpcm_encoded_size(0) == 0);
    puts("ok  encoded_size 与固定向量长度一致");
}

static void test_matches_reference_implementation(void)
{
    qpq_adpcm_stream_t stream;
    qpq_adpcm_stream_open(&stream, qpq_adpcm_fixture_encoded,
                          QPQ_ADPCM_FIXTURE_SAMPLES);

    const uint32_t produced =
        qpq_adpcm_stream_read(&stream, s_bulk, QPQ_ADPCM_FIXTURE_SAMPLES);
    assert(produced == QPQ_ADPCM_FIXTURE_SAMPLES);

    for (uint32_t index = 0; index < QPQ_ADPCM_FIXTURE_SAMPLES; index++) {
        if (s_bulk[index] != qpq_adpcm_fixture_expected[index]) {
            printf("FAIL 样本 %u：C 解出 %d，参考实现是 %d\n", index,
                   (int)s_bulk[index], (int)qpq_adpcm_fixture_expected[index]);
            assert(0);
        }
    }
    assert(qpq_adpcm_stream_done(&stream));
    // 读完再读，只能得到 0。
    assert(qpq_adpcm_stream_read(&stream, s_small, 4) == 0);
    puts("ok  C 解码器与 Python 参考实现逐位一致");
}

static void test_first_sample_is_lossless(void)
{
    assert(s_bulk[0] == qpq_adpcm_fixture_source[0]);
    puts("ok  首样本无损（头部直接存的是原始值）");
}

static void test_chunk_size_does_not_change_result(void)
{
    // 用几种互不整除的分块大小重放，结果必须与一次读完完全相同。
    for (uint32_t chunk = 1; chunk <= 7; chunk++) {
        qpq_adpcm_stream_t stream;
        qpq_adpcm_stream_open(&stream, qpq_adpcm_fixture_encoded,
                              QPQ_ADPCM_FIXTURE_SAMPLES);
        uint32_t total = 0;
        uint32_t guard = 0;
        while (total < QPQ_ADPCM_FIXTURE_SAMPLES && guard++ < 1000) {
            const uint32_t got = qpq_adpcm_stream_read(
                &stream, s_chunked + total, chunk);
            if (got == 0) break;
            total += got;
        }
        assert(total == QPQ_ADPCM_FIXTURE_SAMPLES);
        assert(memcmp(s_chunked, s_bulk, sizeof(s_bulk)) == 0);
    }
    puts("ok  分块读取与一次读完结果相同（流式状态机正确）");
}

static void test_rewind_replays_identically(void)
{
    qpq_adpcm_stream_t stream;
    qpq_adpcm_stream_open(&stream, qpq_adpcm_fixture_encoded,
                          QPQ_ADPCM_FIXTURE_SAMPLES);
    assert(qpq_adpcm_stream_read(&stream, s_chunked, 10) == 10);
    // 只读了一半就回到开头：BGM 循环就是靠这条。
    qpq_adpcm_stream_rewind(&stream);
    assert(qpq_adpcm_stream_read(&stream, s_chunked, QPQ_ADPCM_FIXTURE_SAMPLES) ==
           QPQ_ADPCM_FIXTURE_SAMPLES);
    assert(memcmp(s_chunked, s_bulk, sizeof(s_bulk)) == 0);
    puts("ok  rewind 后可完整重放");
}

static void test_nibble_decoder_matches_sequence(void)
{
    // 按参考实现的步进方式手动走一遍：头部给出首样本与初始步长索引，其余每个
    // 样本一个 nibble，先高位后低位。
    int predictor = (int)(int16_t)((uint16_t)qpq_adpcm_fixture_encoded[0] |
                                   ((uint16_t)qpq_adpcm_fixture_encoded[1] << 8));
    int step_index = qpq_adpcm_fixture_encoded[2];
    assert(predictor == qpq_adpcm_fixture_source[0]);

    uint32_t produced = 1;
    assert(predictor == qpq_adpcm_fixture_expected[0]);
    for (uint32_t offset = QPQ_ADPCM_HEADER_BYTES;
         offset < sizeof(qpq_adpcm_fixture_encoded) &&
         produced < QPQ_ADPCM_FIXTURE_SAMPLES;
         offset++) {
        const uint8_t packed = qpq_adpcm_fixture_encoded[offset];
        predictor = qpq_adpcm_decode_nibble(
            packed >> 4, predictor, step_index, &step_index);
        assert(predictor == qpq_adpcm_fixture_expected[produced]);
        produced++;
        if (produced >= QPQ_ADPCM_FIXTURE_SAMPLES) break;
        predictor = qpq_adpcm_decode_nibble(
            packed & 0x0F, predictor, step_index, &step_index);
        assert(predictor == qpq_adpcm_fixture_expected[produced]);
        produced++;
    }
    assert(produced == QPQ_ADPCM_FIXTURE_SAMPLES);
    puts("ok  nibble 级解码与参考实现同序");
}

static void test_reconstruction_error_is_bounded(void)
{
    int worst = 0;
    int worst_at = 0;
    for (uint32_t index = 0; index < FIXTURE_SIGNAL_SAMPLES; index++) {
        const int error =
            (int)s_bulk[index] - (int)qpq_adpcm_fixture_source[index];
        const int magnitude = error < 0 ? -error : error;
        if (magnitude > worst) {
            worst = magnitude;
            worst_at = (int)index;
        }
    }
    printf("ok  衰减正弦段最大误差 %d（样本 %d，阈值 %d）\n",
           worst, worst_at, FIXTURE_SIGNAL_MAX_ERROR);
    assert(worst <= FIXTURE_SIGNAL_MAX_ERROR);
}

static void test_degenerate_inputs(void)
{
    qpq_adpcm_stream_t stream;

    // 空指针：清空状态，read 返回 0。
    qpq_adpcm_stream_open(&stream, NULL, 16);
    assert(qpq_adpcm_stream_read(&stream, s_small, 4) == 0);
    assert(qpq_adpcm_stream_done(&stream));

    // 样本数为 0：立刻就是「读完」。
    qpq_adpcm_stream_open(&stream, qpq_adpcm_fixture_encoded, 0);
    assert(qpq_adpcm_stream_done(&stream));
    assert(qpq_adpcm_stream_read(&stream, s_small, 4) == 0);

    // 请求 0 个样本：不产出，也不推进位置。
    qpq_adpcm_stream_open(&stream, qpq_adpcm_fixture_encoded, 4);
    assert(qpq_adpcm_stream_read(&stream, s_small, 0) == 0);
    assert(!qpq_adpcm_stream_done(&stream));
    assert(qpq_adpcm_stream_read(&stream, s_small, 4) == 4);

    // 空指针参数不应崩溃。
    assert(qpq_adpcm_stream_read(NULL, s_small, 4) == 0);
    assert(qpq_adpcm_stream_read(&stream, NULL, 4) == 0);
    qpq_adpcm_stream_rewind(NULL);
    assert(qpq_adpcm_stream_done(NULL));
    puts("ok  退化输入（空指针 / 零样本 / 零请求）行为明确");
}

int main(void)
{
    test_encoded_size_matches_fixture();
    test_matches_reference_implementation();
    test_first_sample_is_lossless();
    test_chunk_size_does_not_change_result();
    test_rewind_replays_identically();
    test_nibble_decoder_matches_sequence();
    test_reconstruction_error_is_bounded();
    test_degenerate_inputs();
    puts("test_qpq_adpcm: PASS");
    return 0;
}
