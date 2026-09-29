// main/qpq_audio_blob.c —— 见 qpq_audio_blob.h。
//
// 这是整个应用里唯一出现链接器 asm 符号名的地方。
#include "qpq_audio_blob.h"

// 符号名由 main/CMakeLists.txt 里 target_add_binary_data 的**文件名**决定：
// assets/audio/qpq_audio.bin -> _binary_qpq_audio_bin_start / _end。
// 改文件名就必须同时改这里，所以路径写死在这里而不是散落各处。
//
// 用 asm 标签而不是定义一个同名数组：符号定义在链接期由 objcopy 产物提供，
// 这里只做声明。数组声明后跟 asm 标签在 clang 上不被接受，所以本文件不参与
// 宿主测试 —— 这是把「字节从哪来」与「怎么解析字节」分开的直接原因。
extern const uint8_t qpq_audio_blob_start[] asm("_binary_qpq_audio_bin_start");
extern const uint8_t qpq_audio_blob_end[] asm("_binary_qpq_audio_bin_end");

const uint8_t *qpq_audio_blob(void)
{
    return qpq_audio_blob_start;
}

uint32_t qpq_audio_blob_size(void)
{
    return (uint32_t)(qpq_audio_blob_end - qpq_audio_blob_start);
}
