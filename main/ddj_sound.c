// main/ddj_sound.c —— 见 ddj_sound.h。
#include "ddj_sound.h"

#include "esp_log.h"
#include "rtttl_player.h"

static const char *TAG = "ddj_sound";

// 每个音效都是一段 RTTTL。音区特意压低（o=4 一带）、速度放慢：这台机器是放在
// 床头静读用的，提示音要像翻纸，不像游戏机。
static const char *const SONGS[] = {
    [DDJ_SOUND_MOVE] = "move:d=16,o=5,b=180:a",
    [DDJ_SOUND_ENTER] = "enter:d=16,o=4,b=180:c,e",
    [DDJ_SOUND_BACK] = "back:d=16,o=4,b=180:e,c",
    [DDJ_SOUND_SEAL] = "seal:d=8,o=4,b=150:c,e,g,2p",
    [DDJ_SOUND_BLOCK] = "block:d=32,o=3,b=180:c",
};

#define SONG_COUNT (sizeof(SONGS) / sizeof(SONGS[0]))

static bool s_ready;
static bool s_enabled = true;

esp_err_t ddj_sound_init(void)
{
    if (s_ready) return ESP_OK;
    const esp_err_t err = rtttl_player_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "提示音不可用: %s", esp_err_to_name(err));
        return err;
    }
    s_ready = true;
    return ESP_OK;
}

void ddj_sound_play(ddj_sound_t sound)
{
    if (!s_ready || !s_enabled) return;
    if ((size_t)sound >= SONG_COUNT) return;
    const char *song = SONGS[sound];
    if (!song) return;
    // 队列满只代表上一个音还没播完，不是错误。
    (void)rtttl_player_play(song);
}

void ddj_sound_set_enabled(bool enabled)
{
    s_enabled = enabled;
}

bool ddj_sound_enabled(void)
{
    return s_enabled;
}

bool ddj_sound_ready(void)
{
    return s_ready;
}
