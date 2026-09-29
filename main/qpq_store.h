// main/qpq_store.h —— 存档的 NVS 落盘。
//
// 只做一件事：把 main/qpq_progress.c 定义的那块字节存进 NVS、再读回来。格式的
// 校验、拒绝路径、版本兼容全部在纯逻辑层（qpq_progress），这里不重复实现 ——
// 写盘的代码越薄，掉电时可能出错的地方就越少。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "qpq_progress.h"

// 打开命名空间。失败时应用仍可运行，只是不记进度。
esp_err_t qpq_store_init(void);

// 读存档。首次开机（分区里还没有记录）**不算失败**：会把 progress 复位成空档
// 并返回 ESP_OK。返回非 OK 表示真的出了问题（命名空间打不开、存档校验不过），
// 此时 progress 也被复位成空档，调用方无需区分。
esp_err_t qpq_store_load(qpq_progress_t *progress);

// 写存档。写入后立刻 commit，所以掉电最多丢这一次，不会丢上一次。
esp_err_t qpq_store_save(const qpq_progress_t *progress);

// 音量档位的持久化。它不属于进度，所以是独立的键 —— 也正是为了不碰进度 blob 的
// 格式（那个格式带版本与校验和，加一个字段就得升版本，升版本会让旧存档整体作废、
// 丢掉读者的记录）。
esp_err_t qpq_store_load_volume(uint8_t *percent);
esp_err_t qpq_store_save_volume(uint8_t percent);

// 擦掉本应用的全部记录（设置页的重置进度用）。
esp_err_t qpq_store_erase(void);
