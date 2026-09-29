// main/qpq_store.c —— 见 qpq_store.h。
#include "qpq_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "qpq_store";

// 命名空间与另一个应用（三字经 / 道德经日课）刻意不同名：同机刷过别的应用时，
// 各家的进度互不覆盖。
static const char *NAMESPACE = "qiaopi";
static const char *KEY_PROGRESS = "progress";
static const char *KEY_AUDIO = "audio";

esp_err_t qpq_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 分区需要重建：这是恢复路径，不是正常路径，所以只在确有需要时才擦。
        ESP_LOGW(TAG, "NVS 需要重建（%s），擦除后重试", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err != ESP_OK) return err;
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t qpq_store_load(qpq_progress_t *progress)
{
    if (!progress) return ESP_ERR_INVALID_ARG;
    qpq_progress_reset(progress);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    uint8_t blob[QPQ_PROGRESS_BLOB_SIZE];
    size_t size = sizeof(blob);
    err = nvs_get_blob(handle, KEY_PROGRESS, blob, &size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 首次开机：分区里还没有记录。这是正常路径，不是错误，所以返回 OK
        // 并把进度留成复位后的空档 —— 让上层去区分「没有」和「坏了」只会
        // 逼上层也去 include NVS 的头文件。
        ESP_LOGI(TAG, "首次开机，从空档开始");
        return ESP_OK;
    }
    if (err != ESP_OK) return err;

    const qpq_progress_status_t status =
        qpq_progress_deserialize(progress, blob, (uint32_t)size);
    if (status != QPQ_PROGRESS_OK) {
        // 存档坏了就回到空档，并且明确报出来。这里不尝试「修一修接着用」：
        // 把半截数据当事实读进来，比丢一次成绩糟得多。
        ESP_LOGW(TAG, "存档不可用（%s），已回到空档", qpq_progress_status_name(status));
        qpq_progress_reset(progress);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t qpq_store_save(const qpq_progress_t *progress)
{
    if (!progress) return ESP_ERR_INVALID_ARG;

    uint8_t blob[QPQ_PROGRESS_BLOB_SIZE];
    const uint32_t written = qpq_progress_serialize(progress, blob, sizeof(blob));
    if (written != QPQ_PROGRESS_BLOB_SIZE) return ESP_ERR_INVALID_SIZE;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, KEY_PROGRESS, blob, written);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) ESP_LOGW(TAG, "存档写入失败：%s", esp_err_to_name(err));
    return err;
}

esp_err_t qpq_store_load_audio_enabled(bool *enabled)
{
    if (!enabled) return ESP_ERR_INVALID_ARG;
    *enabled = true;               // 默认开

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    uint8_t value = 1;
    err = nvs_get_u8(handle, KEY_AUDIO, &value);
    nvs_close(handle);
    if (err != ESP_OK) return err;

    *enabled = value != 0;
    return ESP_OK;
}

esp_err_t qpq_store_save_audio_enabled(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(handle, KEY_AUDIO, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t qpq_store_erase(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_erase_all(handle);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
