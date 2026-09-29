// 由 tools/qiaopi/gen_audio.py 生成，请勿手改。
// blob 里的索引表是权威来源；这里只声明编号常量与符号，供固件直接引用。

#pragma once

#include <stdint.h>

/* 片段数 = 题目数 + 1（BGM）。narration 片段下标与题目下标一一对应。 */
#define QPQ_AUDIO_CLIP_COUNT 92
#define QPQ_AUDIO_NARRATION_COUNT 91
#define QPQ_AUDIO_BGM_CLIP 91
#define QPQ_AUDIO_SAMPLE_RATE 16000

/* blob 本身由 main/CMakeLists.txt 的 target_add_binary_data 编进固件；
 * 取字节的入口是 main/qpq_audio_blob.c 里的 qpq_audio_blob() 与
 * qpq_audio_blob_size()。链接器的 asm 符号名只出现在那一个文件里 ——
 * 宿主测试不该、也无法依赖它。 */
