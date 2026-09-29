// main/qpq_audio_index.h —— ADPCM blob 的索引访问。
//
// blob 由 tools/qiaopi/gen_audio.py 生成，自带索引表，所以固件不需要在编译期
// 硬编码任何偏移：打开时校验一次头部，之后按片段号寻址。
//
// 所有多字节字段都按字节拼装，不做对齐转换。blob 由链接器摆在 Flash 的 rodata
// 里，这里不去假设它满足 uint16_t 对齐；RISC-V 上未对齐访问会直接异常。
//
// 本文件不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define QPQ_AUDIO_BLOB_HEADER_BYTES 16
#define QPQ_AUDIO_INDEX_ENTRY_BYTES 8

typedef enum {
    QPQ_AUDIO_OK = 0,
    QPQ_AUDIO_ERR_NULL,        // 传入空指针
    QPQ_AUDIO_ERR_TRUNCATED,   // 长度连头部（或索引）都放不下
    QPQ_AUDIO_ERR_MAGIC,       // 魔数不符，多半是刷错了固件
    QPQ_AUDIO_ERR_VERSION,     // 生成物版本与固件不符
    QPQ_AUDIO_ERR_RATE,        // 采样率不符
    QPQ_AUDIO_ERR_CLIP_RANGE,  // 片段号越界
    QPQ_AUDIO_ERR_LAYOUT,      // 索引与 blob 长度不自洽
} qpq_audio_status_t;

typedef struct {
    const uint8_t *blob;
    uint32_t length;
    uint16_t clip_count;
    uint32_t sample_rate;
} qpq_audio_index_t;

// 校验并打开索引。失败时 index 一律被置为无效状态（哪怕它此前打开成功过），
// 返回值说明失败原因 —— 忽略返回值的调用方也不会拿到一组过期的指针。
qpq_audio_status_t qpq_audio_index_open(qpq_audio_index_t *index,
                                        const uint8_t *blob, uint32_t length);

// 片段号是否有效。
bool qpq_audio_index_valid_clip(const qpq_audio_index_t *index, uint16_t clip);

// 片段样本数；片段号无效时返回 0。
uint32_t qpq_audio_index_samples(const qpq_audio_index_t *index, uint16_t clip);

// 片段数据起始指针（指向 4 字节片段头）；片段号无效时返回 NULL。
const uint8_t *qpq_audio_index_clip(const qpq_audio_index_t *index, uint16_t clip);

// 状态名的静态字符串，供日志使用。
const char *qpq_audio_status_name(qpq_audio_status_t status);
