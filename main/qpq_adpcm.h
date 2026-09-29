// main/qpq_adpcm.h —— IMA-ADPCM 流式解码器。
//
// 与 tools/qiaopi/adpcm.py 的参考实现逐位对应；两者的等价性由
// tests/test_qpq_adpcm.c 用生成器产出的固定向量断言，不是靠约定。
//
// 为什么是流式而不是「一次解完」：最长的片段是 89 秒的 BGM，解成 16 位 PCM 是
// 2.8 MB，而本机只有 400 KB SRAM 且无 PSRAM。播放器一次只解一小块（几百个样本）
// 就写进 I2S，峰值内存就是那一小块栈缓冲。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 片段头部：int16 首样本 + uint8 初始步长索引 + uint8 预留。
#define QPQ_ADPCM_HEADER_BYTES 4

// 步长索引的取值范围，对应 adpcm.py 里的 STEP_TABLE[89]。
#define QPQ_ADPCM_STEP_INDEX_MAX 88

typedef struct {
    const uint8_t *clip;      // 片段起始字节
    uint32_t sample_count;    // 片段总样本数
    uint32_t next_sample;     // 下一个要产出的样本下标
    int predictor;            // 当前预测值，开流时是首样本
    int step_index;           // 当前步长索引
} qpq_adpcm_stream_t;

// N 个样本编码后占多少字节。与 adpcm.encoded_size 一致。
size_t qpq_adpcm_encoded_size(uint32_t sample_count);

// 打开一条流。clip 至少要能放下 QPQ_ADPCM_HEADER_BYTES 字节；
// sample_count 为 0 时视为空流，read 直接返回 0。
void qpq_adpcm_stream_open(qpq_adpcm_stream_t *stream, const uint8_t *clip,
                           uint32_t sample_count);

// 回到片段开头，供 BGM 循环用。
void qpq_adpcm_stream_rewind(qpq_adpcm_stream_t *stream);

// 最多产出 max_samples 个样本，返回实际产出数。到片段末尾时返回值会小于请求量，
// 之后继续调用只会返回 0。
uint32_t qpq_adpcm_stream_read(qpq_adpcm_stream_t *stream, int16_t *out,
                               uint32_t max_samples);

// 片段是否已经全部产出。
bool qpq_adpcm_stream_done(const qpq_adpcm_stream_t *stream);

// 解码一个 nibble。暴露出来是为了让宿主测试能按参考实现的步进方式逐位比对。
// 返回值是新的预测值，步长索引通过 out_step_index 回传。
int qpq_adpcm_decode_nibble(int nibble, int predictor, int step_index,
                            int *out_step_index);
