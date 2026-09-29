// main/ddj_store.h —— 读经进度与设置的持久化（NVS）。
//
// 只做读写；进度本身的结构与规则在 ddj_progress 里（纯逻辑，可宿主测试）。
#pragma once

#include "esp_err.h"
#include "ddj_progress.h"

// 初始化 NVS 并载入已有进度；无记录或记录损坏时从零开始（不阻塞应用启动）。
esp_err_t ddj_store_init(void);

// 可写的进度对象。改动后调用 ddj_store_save() 落盘。
ddj_progress_t *ddj_store_progress(void);

// 写进 NVS。
esp_err_t ddj_store_save(void);

// 清除记录并复位为初始进度。
esp_err_t ddj_store_reset(void);

// ---- 音效开关 ----
// 单独一个键保存：开一次静音不该动到读经进度那条记录。
// 没有记录时视为开启。
bool ddj_store_audio_enabled(void);
esp_err_t ddj_store_set_audio_enabled(bool enabled);
