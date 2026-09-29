// main/qpq_audio_blob.h —— 音频 blob 在固件里的入口。
//
// 这一层只干一件事：把「生成器编出来的那一大块字节」交给上层。它只参与固件构建，
// 因为取字节靠的是链接器的 asm 符号名（由 target_add_binary_data 按文件名生成），
// 那个名字在宿主机上不存在、也不该被测试依赖。
//
// 为什么不把 blob 生成为一个巨大的 .c 数组：5 MB 的数组会让编译和链接明显变慢，
// 而且生成出来的源文件无法审阅。二进制附加（target_add_binary_data）没有这两个
// 问题，字节仍然落在 Flash 的 rodata 段，读它就是一次内存映射访问。
#pragma once

#include <stdint.h>

// blob 起始地址。生命周期直到进程结束，不需要释放。
const uint8_t *qpq_audio_blob(void);

// blob 总字节数。
uint32_t qpq_audio_blob_size(void);
