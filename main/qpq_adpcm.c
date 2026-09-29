// main/qpq_adpcm.c —— IMA-ADPCM 流式解码器，见 qpq_adpcm.h。
// 本文件刻意不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#include "qpq_adpcm.h"

// IMA 步长表，89 项，与 tools/qiaopi/adpcm.py 的 STEP_TABLE 逐项相同。
static const int s_step_table[QPQ_ADPCM_STEP_INDEX_MAX + 1] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

// 步长索引调整表，用 nibble 的低三位索引。
static const int s_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,
};

static int clamp_predictor(int value)
{
    if (value < -32768) return -32768;
    if (value > 32767) return 32767;
    return value;
}

static int clamp_step_index(int value)
{
    if (value < 0) return 0;
    if (value > QPQ_ADPCM_STEP_INDEX_MAX) return QPQ_ADPCM_STEP_INDEX_MAX;
    return value;
}

size_t qpq_adpcm_encoded_size(uint32_t sample_count)
{
    if (sample_count == 0) return 0;
    // 4 字节头 + ceil((sample_count - 1) / 2) 字节半字节流。
    return (size_t)QPQ_ADPCM_HEADER_BYTES + ((size_t)sample_count - 1U + 1U) / 2U;
}

int qpq_adpcm_decode_nibble(int nibble, int predictor, int step_index,
                            int *out_step_index)
{
    const int step = s_step_table[clamp_step_index(step_index)];

    int reconstructed = step >> 3;
    if (nibble & 4) reconstructed += step;
    if (nibble & 2) reconstructed += step >> 1;
    if (nibble & 1) reconstructed += step >> 2;

    if (nibble & 8) {
        predictor -= reconstructed;
    } else {
        predictor += reconstructed;
    }

    if (out_step_index) {
        *out_step_index = clamp_step_index(step_index + s_index_table[nibble & 7]);
    }
    return clamp_predictor(predictor);
}

void qpq_adpcm_stream_open(qpq_adpcm_stream_t *stream, const uint8_t *clip,
                           uint32_t sample_count)
{
    if (!stream) return;
    stream->clip = clip;
    stream->sample_count = clip ? sample_count : 0;
    // 首样本也算一个要产出的样本：把它留在 next_sample == 0，由 read() 从
    // 头部里的预测值直接吐出。设成 1 会静默少发一个样本。
    stream->next_sample = 0;
    // 头部字段按字节拼，不做对齐转换：blob 在 Flash 里由链接器摆放，这里不去
    // 假设它的对齐满足 uint16_t 读取。RISC-V 上未对齐访问会直接异常。
    stream->predictor = clip
        ? (int)(int16_t)((uint16_t)clip[0] | ((uint16_t)clip[1] << 8))
        : 0;
    stream->step_index = clip ? clamp_step_index(clip[2]) : 0;
}

void qpq_adpcm_stream_rewind(qpq_adpcm_stream_t *stream)
{
    if (!stream || !stream->clip) return;
    qpq_adpcm_stream_open(stream, stream->clip, stream->sample_count);
}

uint32_t qpq_adpcm_stream_read(qpq_adpcm_stream_t *stream, int16_t *out,
                               uint32_t max_samples)
{
    if (!stream || !out || !stream->clip) return 0;

    uint32_t produced = 0;
    while (produced < max_samples && stream->next_sample < stream->sample_count) {
        if (stream->next_sample == 0) {
            // 首样本直接就是头部里存的那个值。
            out[produced++] = (int16_t)stream->predictor;
            stream->next_sample = 1;
            continue;
        }

        const uint32_t nibble_index = stream->next_sample - 1U;
        const uint8_t packed =
            stream->clip[QPQ_ADPCM_HEADER_BYTES + (size_t)(nibble_index >> 1)];
        const int nibble = (nibble_index & 1U) ? (packed & 0x0F) : (packed >> 4);

        stream->predictor = qpq_adpcm_decode_nibble(
            nibble, stream->predictor, stream->step_index, &stream->step_index);
        out[produced++] = (int16_t)stream->predictor;
        stream->next_sample++;
    }
    return produced;
}

bool qpq_adpcm_stream_done(const qpq_adpcm_stream_t *stream)
{
    if (!stream || !stream->clip) return true;
    return stream->next_sample >= stream->sample_count;
}
