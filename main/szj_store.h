// main/szj_store.h —— 学习进度的持久化(NVS)。
//
// 只做读写;进度本身的结构与规则在 szj_progress 里(纯逻辑,可宿主测试)。
#pragma once

#include "esp_err.h"
#include "szj_progress.h"

// 初始化 NVS 并载入已有进度;无记录或记录损坏时从零开始(不阻塞应用启动)。
esp_err_t szj_store_init(void);

// 可写的进度对象。改动后调用 szj_store_save() 落盘。
szj_progress_t *szj_store_progress(void);

// 写入 NVS。
esp_err_t szj_store_save(void);

// 清除记录并复位为初始进度。
esp_err_t szj_store_reset(void);

// ---- 音效开关 ----
// 单独一个键保存:开一次静音不该动到学习进度那条记录。
// 没有记录时视为开启(默认有声音)。
bool szj_store_audio_enabled(void);
esp_err_t szj_store_set_audio_enabled(bool enabled);
