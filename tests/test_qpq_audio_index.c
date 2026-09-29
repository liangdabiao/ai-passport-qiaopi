// tests/test_qpq_audio_index.c —— ADPCM blob 索引的解析与拒绝路径。
//
// 分两半：
//   1. 用内存里手工搭出来的小 blob 覆盖解析器的每条拒绝路径 —— 刷了半截固件、
//      生成物版本不符、索引与长度不自洽，这些必须在打开时就失败，而不是等到播放
//      某一道题时读到别处去。
//   2. 若 assets/audio/qpq_audio.bin 在位，直接打开**真实产物**核对片段数与编号，
//      这是生成器与解析器之间的集成检查。文件不在时明确跳过并说明，不假装通过。
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qpq_adpcm.h"
#include "qpq_audio.h"
#include "qpq_audio_index.h"

// 两个片段的合成 blob：5 个样本与 4 个样本。
#define FAKE_CLIP0_SAMPLES 5u
#define FAKE_CLIP1_SAMPLES 4u
// 片段字节数写成字面量而不是调用 encoded_size()：那个函数不是编译期常量，用在
// 文件作用域的数组尺寸上会变成变长数组。下面的测试里有一条断言专门核对这两个
// 字面量与函数的返回值一致，所以这里的复述不会漂移。
#define FAKE_CLIP0_BYTES 6u   // 4 字节头 + ceil((5-1)/2)
#define FAKE_CLIP1_BYTES 6u   // 4 字节头 + ceil((4-1)/2)
#define FAKE_INDEX_BYTES (QPQ_AUDIO_BLOB_HEADER_BYTES + 2u * QPQ_AUDIO_INDEX_ENTRY_BYTES)
#define FAKE_TOTAL_BYTES (FAKE_INDEX_BYTES + FAKE_CLIP0_BYTES + FAKE_CLIP1_BYTES)

static uint8_t s_blob[FAKE_TOTAL_BYTES + 16];
static qpq_audio_index_t s_index;

static void put_u16(uint8_t *at, uint16_t value)
{
    at[0] = (uint8_t)(value & 0xFFu);
    at[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_u32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value & 0xFFu);
    at[1] = (uint8_t)((value >> 8) & 0xFFu);
    at[2] = (uint8_t)((value >> 16) & 0xFFu);
    at[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void build_valid_blob(void)
{
    memset(s_blob, 0, sizeof(s_blob));
    s_blob[0] = 'Q';
    s_blob[1] = 'P';
    s_blob[2] = 'Q';
    s_blob[3] = 'A';
    put_u16(s_blob + 4, 1);                                  // version
    put_u16(s_blob + 6, 2);                                  // clip_count
    put_u32(s_blob + 8, QPQ_AUDIO_SAMPLE_RATE);              // sample_rate
    put_u32(s_blob + 12, 0);                                 // reserved

    const uint32_t clip0_offset = FAKE_INDEX_BYTES;
    const uint32_t clip1_offset =
        clip0_offset + (uint32_t)qpq_adpcm_encoded_size(FAKE_CLIP0_SAMPLES);
    put_u32(s_blob + QPQ_AUDIO_BLOB_HEADER_BYTES, clip0_offset);
    put_u32(s_blob + QPQ_AUDIO_BLOB_HEADER_BYTES + 4, FAKE_CLIP0_SAMPLES);
    put_u32(s_blob + QPQ_AUDIO_BLOB_HEADER_BYTES + QPQ_AUDIO_INDEX_ENTRY_BYTES,
            clip1_offset);
    put_u32(s_blob + QPQ_AUDIO_BLOB_HEADER_BYTES + QPQ_AUDIO_INDEX_ENTRY_BYTES + 4,
            FAKE_CLIP1_SAMPLES);
}

static qpq_audio_status_t open_corrupted(void (*corrupt)(uint8_t *blob, uint32_t *length),
                                         uint32_t *out_length)
{
    build_valid_blob();
    uint32_t length = FAKE_TOTAL_BYTES;
    if (corrupt) corrupt(s_blob, &length);
    if (out_length) *out_length = length;
    return qpq_audio_index_open(&s_index, s_blob, length);
}

static void test_valid_blob_opens(void)
{
    // 先确认上面的字面量与编码器的真实长度一致，避免两处数字漂移。
    assert(qpq_adpcm_encoded_size(FAKE_CLIP0_SAMPLES) == FAKE_CLIP0_BYTES);
    assert(qpq_adpcm_encoded_size(FAKE_CLIP1_SAMPLES) == FAKE_CLIP1_BYTES);

    const qpq_audio_status_t status = open_corrupted(NULL, NULL);
    assert(status == QPQ_AUDIO_OK);
    assert(s_index.clip_count == 2);
    assert(s_index.sample_rate == (uint32_t)QPQ_AUDIO_SAMPLE_RATE);
    assert(s_index.length == FAKE_TOTAL_BYTES);

    assert(qpq_audio_index_valid_clip(&s_index, 0));
    assert(qpq_audio_index_valid_clip(&s_index, 1));
    assert(!qpq_audio_index_valid_clip(&s_index, 2));
    assert(qpq_audio_index_samples(&s_index, 0) == FAKE_CLIP0_SAMPLES);
    assert(qpq_audio_index_samples(&s_index, 1) == FAKE_CLIP1_SAMPLES);
    assert(qpq_audio_index_samples(&s_index, 2) == 0);

    const uint8_t *clip0 = qpq_audio_index_clip(&s_index, 0);
    const uint8_t *clip1 = qpq_audio_index_clip(&s_index, 1);
    assert(clip0 == s_blob + FAKE_INDEX_BYTES);
    assert(clip1 == clip0 + qpq_adpcm_encoded_size(FAKE_CLIP0_SAMPLES));
    assert(qpq_audio_index_clip(&s_index, 2) == NULL);

    // 索引指向的字节确实能被解码器读出来。
    qpq_adpcm_stream_t stream;
    qpq_adpcm_stream_open(&stream, clip0, FAKE_CLIP0_SAMPLES);
    int16_t samples[FAKE_CLIP0_SAMPLES];
    assert(qpq_adpcm_stream_read(&stream, samples, FAKE_CLIP0_SAMPLES) ==
           FAKE_CLIP0_SAMPLES);
    puts("ok  合法 blob 打开成功，片段寻址与解码通路打通");
}

static void corrupt_magic(uint8_t *blob, uint32_t *length)
{
    (void)length;
    blob[0] = 'X';
}

static void corrupt_version(uint8_t *blob, uint32_t *length)
{
    (void)length;
    blob[4] = 2;
}

static void corrupt_rate(uint8_t *blob, uint32_t *length)
{
    (void)length;
    put_u32(blob + 8, 22050);
}

static void corrupt_zero_samples(uint8_t *blob, uint32_t *length)
{
    (void)length;
    put_u32(blob + QPQ_AUDIO_BLOB_HEADER_BYTES + 4, 0);
}

static void corrupt_offset(uint8_t *blob, uint32_t *length)
{
    (void)length;
    put_u32(blob + QPQ_AUDIO_BLOB_HEADER_BYTES, FAKE_INDEX_BYTES + 4);
}

static void corrupt_last_clip_overruns(uint8_t *blob, uint32_t *length)
{
    (void)length;
    put_u32(blob + QPQ_AUDIO_BLOB_HEADER_BYTES + QPQ_AUDIO_INDEX_ENTRY_BYTES + 4,
            0x00FFFFFFu);
}

static void test_rejection_paths(void)
{
    // 空指针。
    assert(qpq_audio_index_open(NULL, s_blob, FAKE_TOTAL_BYTES) == QPQ_AUDIO_ERR_NULL);
    assert(qpq_audio_index_open(&s_index, NULL, FAKE_TOTAL_BYTES) == QPQ_AUDIO_ERR_NULL);

    // 长度连头部都放不下。
    assert(open_corrupted(NULL, NULL) == QPQ_AUDIO_OK);
    assert(qpq_audio_index_open(&s_index, s_blob, QPQ_AUDIO_BLOB_HEADER_BYTES - 1) ==
           QPQ_AUDIO_ERR_TRUNCATED);

    // 索引区被截断（头部够，但 clip_count 声明的索引放不下）。
    open_corrupted(NULL, NULL);
    assert(qpq_audio_index_open(&s_index, s_blob, QPQ_AUDIO_BLOB_HEADER_BYTES + 4) ==
           QPQ_AUDIO_ERR_TRUNCATED);

    // 魔数不符：多半是刷错了固件。
    assert(open_corrupted(corrupt_magic, NULL) == QPQ_AUDIO_ERR_MAGIC);

    // 版本不符。
    assert(open_corrupted(corrupt_version, NULL) == QPQ_AUDIO_ERR_VERSION);

    // 采样率不符：固件按固定采样率打开 codec，不匹配必须拒绝。
    assert(open_corrupted(corrupt_rate, NULL) == QPQ_AUDIO_ERR_RATE);

    // 片段样本数为 0。
    assert(open_corrupted(corrupt_zero_samples, NULL) == QPQ_AUDIO_ERR_LAYOUT);

    // 偏移不自洽：第一条的偏移跳过了一段。
    assert(open_corrupted(corrupt_offset, NULL) == QPQ_AUDIO_ERR_LAYOUT);

    // 最后一条声明的样本数越出 blob 之外（模拟刷了半截固件）。
    assert(open_corrupted(corrupt_last_clip_overruns, NULL) == QPQ_AUDIO_ERR_LAYOUT);

    // 索引自洽但其后有尾巴：长度不等于索引推出来的结尾。
    build_valid_blob();
    assert(qpq_audio_index_open(&s_index, s_blob, FAKE_TOTAL_BYTES + 1) ==
           QPQ_AUDIO_ERR_LAYOUT);

    // 打开失败后，结构体不能被当作可用。
    assert(!qpq_audio_index_valid_clip(&s_index, 0));
    assert(qpq_audio_index_clip(&s_index, 0) == NULL);
    assert(qpq_audio_index_samples(&s_index, 0) == 0);
    puts("ok  九条拒绝路径（空指针/截断/魔数/版本/采样率/零样本/偏移/越界/长度尾巴）");
}

static void test_status_names_are_all_distinct(void)
{
    const char *names[8];
    names[0] = qpq_audio_status_name(QPQ_AUDIO_OK);
    names[1] = qpq_audio_status_name(QPQ_AUDIO_ERR_NULL);
    names[2] = qpq_audio_status_name(QPQ_AUDIO_ERR_TRUNCATED);
    names[3] = qpq_audio_status_name(QPQ_AUDIO_ERR_MAGIC);
    names[4] = qpq_audio_status_name(QPQ_AUDIO_ERR_VERSION);
    names[5] = qpq_audio_status_name(QPQ_AUDIO_ERR_RATE);
    names[6] = qpq_audio_status_name(QPQ_AUDIO_ERR_CLIP_RANGE);
    names[7] = qpq_audio_status_name(QPQ_AUDIO_ERR_LAYOUT);
    for (int i = 0; i < 8; i++) {
        assert(names[i] != NULL && names[i][0] != '\0');
        for (int j = i + 1; j < 8; j++) {
            assert(strcmp(names[i], names[j]) != 0);
        }
    }
    puts("ok  每种错误状态都有可读且互不相同的名字");
}

static void test_real_blob_if_present(void)
{
    FILE *handle = fopen("assets/audio/qpq_audio.bin", "rb");
    if (!handle) {
        puts("skip: assets/audio/qpq_audio.bin 不在位，未核对真实产物"
             "（先跑 python3 tools/qiaopi/gen_audio.py）");
        return;
    }
    assert(fseek(handle, 0, SEEK_END) == 0);
    const long size = ftell(handle);
    assert(size > 0);
    assert(fseek(handle, 0, SEEK_SET) == 0);

    uint8_t *data = (uint8_t *)malloc((size_t)size);
    assert(data != NULL);
    assert(fread(data, 1, (size_t)size, handle) == (size_t)size);
    fclose(handle);

    const qpq_audio_status_t status =
        qpq_audio_index_open(&s_index, data, (uint32_t)size);
    if (status != QPQ_AUDIO_OK) {
        printf("FAIL 真实 blob 打开失败：%s\n", qpq_audio_status_name(status));
        assert(0);
    }
    assert(s_index.clip_count == (uint16_t)QPQ_AUDIO_CLIP_COUNT);

    // 每个片段都必须能读出非零样本数，且累加长度与 blob 一致。
    uint32_t total_samples = 0;
    for (uint16_t clip = 0; clip < s_index.clip_count; clip++) {
        const uint32_t samples = qpq_audio_index_samples(&s_index, clip);
        assert(samples > 0);
        assert(qpq_audio_index_clip(&s_index, clip) != NULL);
        total_samples += samples;
    }
    assert(total_samples == 10595712u);          // clips.txt 里记的总样本数
    assert(qpq_audio_index_samples(&s_index, (uint16_t)QPQ_AUDIO_BGM_CLIP) ==
           1423232u);                           // BGM 是 89.0 秒

    // 每个片段的头部字节都能被解码器接受，且首样本可读。
    qpq_adpcm_stream_t stream;
    qpq_adpcm_stream_open(&stream,
                          qpq_audio_index_clip(&s_index, 0),
                          qpq_audio_index_samples(&s_index, 0));
    int16_t sample = 0;
    assert(qpq_adpcm_stream_read(&stream, &sample, 1) == 1);

    printf("ok  真实产物核对：%u 个片段，共 %u 个样本（%.1f 秒），%ld 字节\n",
           (unsigned)s_index.clip_count, total_samples,
           (double)total_samples / (double)QPQ_AUDIO_SAMPLE_RATE, size);
    free(data);
}

int main(void)
{
    test_valid_blob_opens();
    test_rejection_paths();
    test_status_names_are_all_distinct();
    test_real_blob_if_present();
    puts("test_qpq_audio_index: PASS");
    return 0;
}
