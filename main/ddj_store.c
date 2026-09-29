// main/ddj_store.c —— 见 ddj_store.h。
#include "ddj_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "ddj_store";
static const char *NAMESPACE = "daodejing";
static const char *KEY = "progress";
static const char *KEY_AUDIO = "audio";

static ddj_progress_t s_progress;
static bool s_audio_enabled = true;
static bool s_ready;

static esp_err_t open_namespace(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    return nvs_open(NAMESPACE, mode, handle);
}

esp_err_t ddj_store_init(void)
{
    ddj_progress_reset(&s_progress);
    s_audio_enabled = true;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 需要重建: %s", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    nvs_handle_t handle;
    err = open_namespace(&handle, NVS_READONLY);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "无历史进度,从零开始");
        s_ready = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "打开进度存储失败: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t blob[DDJ_PROGRESS_BLOB_SIZE];
    size_t length = sizeof(blob);
    err = nvs_get_blob(handle, KEY, blob, &length);

    uint8_t audio = 1;
    const esp_err_t audio_err = nvs_get_u8(handle, KEY_AUDIO, &audio);
    nvs_close(handle);

    if (audio_err == ESP_OK) s_audio_enabled = (audio != 0);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "无历史进度,从零开始");
        s_ready = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "读取进度失败: %s", esp_err_to_name(err));
        return err;
    }

    if (!ddj_progress_deserialize(&s_progress, blob, length)) {
        ESP_LOGW(TAG, "进度记录损坏,已忽略");
        ddj_progress_reset(&s_progress);
    } else {
        ESP_LOGI(TAG, "载入进度: 已读=%d 收下=%d 日课=%d 音效=%d",
                 ddj_progress_read_count(&s_progress),
                 ddj_progress_starred_count(&s_progress),
                 ddj_progress_sessions(&s_progress),
                 (int)s_audio_enabled);
    }
    s_ready = true;
    return ESP_OK;
}

ddj_progress_t *ddj_store_progress(void)
{
    return &s_progress;
}

esp_err_t ddj_store_save(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    uint8_t blob[DDJ_PROGRESS_BLOB_SIZE];
    const size_t length = ddj_progress_serialize(&s_progress, blob, sizeof(blob));
    if (length == 0) return ESP_ERR_INVALID_SIZE;

    nvs_handle_t handle;
    esp_err_t err = open_namespace(&handle, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开进度存储失败: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(handle, KEY, blob, length);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存进度失败: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ddj_store_reset(void)
{
    ddj_progress_reset(&s_progress);
    return ddj_store_save();
}

bool ddj_store_audio_enabled(void)
{
    return s_audio_enabled;
}

esp_err_t ddj_store_set_audio_enabled(bool enabled)
{
    s_audio_enabled = enabled;
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    nvs_handle_t handle;
    esp_err_t err = open_namespace(&handle, NVS_READWRITE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开设置存储失败: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_u8(handle, KEY_AUDIO, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存音效开关失败: %s", esp_err_to_name(err));
    }
    return err;
}
