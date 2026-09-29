// main/qpq_audio_index.c —— 见 qpq_audio_index.h。
// 不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#include "qpq_audio_index.h"

#include "qpq_adpcm.h"
#include "qpq_audio.h"

static uint16_t read_u16(const uint8_t *at)
{
    return (uint16_t)((uint16_t)at[0] | ((uint16_t)at[1] << 8));
}

static uint32_t read_u32(const uint8_t *at)
{
    return (uint32_t)at[0] | ((uint32_t)at[1] << 8) |
           ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}

qpq_audio_status_t qpq_audio_index_open(qpq_audio_index_t *index,
                                        const uint8_t *blob, uint32_t length)
{
    if (!index) return QPQ_AUDIO_ERR_NULL;
    // 先置为无效：任何一条失败出口都不该给调用方留下「上一次成功打开」的状态。
    // 否则忽略返回值的调用方会拿着一组过期指针继续寻址，而那正是最难查的一类
    // 故障（能跑，但读到的是别处）。
    index->blob = NULL;
    index->length = 0;
    index->clip_count = 0;
    index->sample_rate = 0;

    if (!blob) return QPQ_AUDIO_ERR_NULL;
    if (length < QPQ_AUDIO_BLOB_HEADER_BYTES) return QPQ_AUDIO_ERR_TRUNCATED;

    if (blob[0] != 'Q' || blob[1] != 'P' || blob[2] != 'Q' || blob[3] != 'A') {
        return QPQ_AUDIO_ERR_MAGIC;
    }
    if (read_u16(blob + 4) != 1) return QPQ_AUDIO_ERR_VERSION;
    const uint16_t clip_count = read_u16(blob + 6);
    const uint32_t sample_rate = read_u32(blob + 8);
    if (sample_rate != QPQ_AUDIO_SAMPLE_RATE) return QPQ_AUDIO_ERR_RATE;

    // 索引必须完整落在 blob 内。
    const uint32_t index_bytes =
        (uint32_t)QPQ_AUDIO_BLOB_HEADER_BYTES +
        (uint32_t)QPQ_AUDIO_INDEX_ENTRY_BYTES * (uint32_t)clip_count;
    if (index_bytes > length) return QPQ_AUDIO_ERR_TRUNCATED;

    // 逐条核对偏移与长度是否自洽：偏移应当紧跟前一条数据、且整段落在 blob 内。
    // 这一步让「刷了半截固件」或「生成物被截断」在这里就暴露，而不是在播放到
    // 某一道题时读到别处去。
    uint32_t expected_offset = index_bytes;
    for (uint16_t clip = 0; clip < clip_count; clip++) {
        const uint8_t *entry =
            blob + QPQ_AUDIO_BLOB_HEADER_BYTES + (size_t)clip * QPQ_AUDIO_INDEX_ENTRY_BYTES;
        const uint32_t offset = read_u32(entry);
        const uint32_t samples = read_u32(entry + 4);
        if (samples == 0) return QPQ_AUDIO_ERR_LAYOUT;
        if (offset != expected_offset) return QPQ_AUDIO_ERR_LAYOUT;
        const size_t bytes = qpq_adpcm_encoded_size(samples);
        if (offset + bytes > length) return QPQ_AUDIO_ERR_LAYOUT;
        expected_offset = offset + (uint32_t)bytes;
    }
    if (expected_offset != length) return QPQ_AUDIO_ERR_LAYOUT;

    index->blob = blob;
    index->length = length;
    index->clip_count = clip_count;
    index->sample_rate = sample_rate;
    return QPQ_AUDIO_OK;
}

bool qpq_audio_index_valid_clip(const qpq_audio_index_t *index, uint16_t clip)
{
    return index && index->blob && clip < index->clip_count;
}

uint32_t qpq_audio_index_samples(const qpq_audio_index_t *index, uint16_t clip)
{
    if (!qpq_audio_index_valid_clip(index, clip)) return 0;
    const uint8_t *entry =
        index->blob + QPQ_AUDIO_BLOB_HEADER_BYTES + (size_t)clip * QPQ_AUDIO_INDEX_ENTRY_BYTES;
    return read_u32(entry + 4);
}

const uint8_t *qpq_audio_index_clip(const qpq_audio_index_t *index, uint16_t clip)
{
    if (!qpq_audio_index_valid_clip(index, clip)) return NULL;
    const uint8_t *entry =
        index->blob + QPQ_AUDIO_BLOB_HEADER_BYTES + (size_t)clip * QPQ_AUDIO_INDEX_ENTRY_BYTES;
    return index->blob + read_u32(entry);
}

const char *qpq_audio_status_name(qpq_audio_status_t status)
{
    switch (status) {
        case QPQ_AUDIO_OK:            return "ok";
        case QPQ_AUDIO_ERR_NULL:      return "空指针";
        case QPQ_AUDIO_ERR_TRUNCATED: return "blob 被截断";
        case QPQ_AUDIO_ERR_MAGIC:     return "魔数不符";
        case QPQ_AUDIO_ERR_VERSION:   return "生成物版本不符";
        case QPQ_AUDIO_ERR_RATE:      return "采样率不符";
        case QPQ_AUDIO_ERR_CLIP_RANGE:return "片段号越界";
        case QPQ_AUDIO_ERR_LAYOUT:    return "索引与长度不自洽";
        default:                      return "未知错误";
    }
}
