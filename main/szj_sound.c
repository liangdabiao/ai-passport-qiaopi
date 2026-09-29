// main/szj_sound.c —— 见 szj_sound.h。
#include "szj_sound.h"

#include "esp_log.h"
#include "rtttl_player.h"

static const char *TAG = "szj_sound";

// 每个音效都是一段 RTTTL:短促、音高走向能表达含义,孩子不看屏幕也能听出对错。
static const char *const SONGS[] = {
    [SZJ_SOUND_MOVE] = "move:d=32,o=6,b=320:g",
    [SZJ_SOUND_ENTER] = "enter:d=32,o=5,b=280:c,e",
    [SZJ_SOUND_BACK] = "back:d=32,o=5,b=280:e,c",
    [SZJ_SOUND_CORRECT] = "ok:d=16,o=5,b=260:c,e,g",
    [SZJ_SOUND_WRONG] = "no:d=16,o=4,b=220:g,16p,e,16p,c",
    [SZJ_SOUND_LESSON_DONE] = "done:d=8,o=5,b=190:c,e,g,c6,g,c6,4p",
};

#define SONG_COUNT (sizeof(SONGS) / sizeof(SONGS[0]))

static bool s_ready;
static bool s_enabled = true;

esp_err_t szj_sound_init(void) {
    if (s_ready) return ESP_OK;
    const esp_err_t err = rtttl_player_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "音效不可用: %s", esp_err_to_name(err));
        return err;
    }
    s_ready = true;
    return ESP_OK;
}

void szj_sound_play(szj_sound_t sound) {
    if (!s_ready || !s_enabled) return;
    if ((size_t)sound >= SONG_COUNT) return;
    const char *song = SONGS[sound];
    if (!song) return;
    // 队列满只代表上一个音效还没播完,不是错误。
    (void)rtttl_player_play(song);
}

void szj_sound_set_enabled(bool enabled) {
    s_enabled = enabled;
}

bool szj_sound_enabled(void) {
    return s_enabled;
}

bool szj_sound_ready(void) {
    return s_ready;
}
