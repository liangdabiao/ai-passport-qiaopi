// main/qpq_app.c —— 见 qpq_app.h。
#include "qpq_app.h"

#include <stddef.h>
#include <string.h>

#include "bsp_display.h"   // bsp_lvgl_lock / bsp_lvgl_unlock
#include "esp_log.h"
#include "esp_timer.h"

#include "qpq_content.h"
#include "qpq_player.h"
#include "qpq_store.h"

static const char *TAG = "qpq_app";

typedef enum {
    PAGE_NONE = 0,
    PAGE_TITLE,
    PAGE_ASK,
    PAGE_REVEAL,
    PAGE_SUMMARY,
} page_id_t;

static qpq_session_t s_session;
static qpq_progress_t s_progress;
static bool s_audio_enabled = true;
static int64_t s_run_start_us;

static lv_obj_t *s_screen;
static page_id_t s_page = PAGE_NONE;

// 当前页面的离开钩子。跳页时先调它清掉旧页面的静态状态，再建新页面 ——
// 顺序反过来会让旧页面的 leave 去清已经属于新页面的状态。
static void (*s_leave)(void);

static void seed_session(void)
{
    // 种子取上电后的微秒数：每次开机不同，同一局里又完全确定（可复现由测试保证）。
    qpq_session_init(&s_session, (uint32_t)esp_timer_get_time());
    qpq_session_set_seen(&s_session, s_progress.seen);
}

static void replace_page(page_id_t page, lv_obj_t *(*enter)(void), void (*leave)(void))
{
    if (s_leave) s_leave();
    lv_obj_t *next = enter();
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = next;
    s_page = page;
    s_leave = leave;
    lv_scr_load(s_screen);
}

void qpq_app_goto_title(void)
{
    replace_page(PAGE_TITLE, qpq_page_title_enter, qpq_page_title_leave);
}

void qpq_app_goto_ask(void)
{
    replace_page(PAGE_ASK, qpq_page_ask_enter, qpq_page_ask_leave);
}

void qpq_app_goto_reveal(void)
{
    replace_page(PAGE_REVEAL, qpq_page_reveal_enter, qpq_page_reveal_leave);
}

void qpq_app_goto_summary(void)
{
    replace_page(PAGE_SUMMARY, qpq_page_summary_enter, qpq_page_summary_leave);
}

qpq_session_t *qpq_app_session(void)
{
    return &s_session;
}

const qpq_progress_t *qpq_app_progress(void)
{
    return &s_progress;
}

bool qpq_app_audio_enabled(void)
{
    return s_audio_enabled;
}

void qpq_app_set_audio_enabled(bool enabled)
{
    if (s_audio_enabled == enabled) return;
    s_audio_enabled = enabled;
    qpq_player_set_muted(!enabled);
    const esp_err_t err = qpq_store_save_audio_enabled(enabled);
    if (err != ESP_OK) {
        // 存不下来不影响本次使用，只影响下次开机 —— 记一笔就好，不打断用户。
        ESP_LOGW(TAG, "音效开关未能持久化：%s", esp_err_to_name(err));
    }
}

int qpq_app_elapsed_seconds(void)
{
    if (s_run_start_us == 0) return 0;
    return (int)((esp_timer_get_time() - s_run_start_us) / 1000000);
}

// 由页面在「开始一局」之后调用：记下起算时刻。
void qpq_app_note_run_started(void)
{
    s_run_start_us = esp_timer_get_time();
}

// 由结算页在记录成绩时调用：写存档并把本局的题目标为已见。
void qpq_app_commit_run(void)
{
    qpq_progress_record_run(&s_progress, &s_session);
    const esp_err_t err = qpq_store_save(&s_progress);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "进度未能保存：%s", esp_err_to_name(err));
    }
}

void qpq_app_reset_progress(void)
{
    qpq_progress_reset(&s_progress);
    const esp_err_t err = qpq_store_erase();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "清空记录未能落盘：%s", esp_err_to_name(err));
    }
}

void qpq_app_key(qpq_key_t key)
{
    // 拿不到锁就直接丢弃这次按键：宁可少响应一次，也不能在没锁的情况下碰 LVGL。
    if (!bsp_lvgl_lock(500)) {
        ESP_LOGW(TAG, "未能取得 LVGL 锁，本次按键被丢弃");
        return;
    }

    switch (s_page) {
        case PAGE_TITLE:   qpq_page_title_key(key); break;
        case PAGE_ASK:     qpq_page_ask_key(key); break;
        case PAGE_REVEAL:  qpq_page_reveal_key(key); break;
        case PAGE_SUMMARY: qpq_page_summary_key(key); break;
        default: break;
    }

    bsp_lvgl_unlock();
}

void qpq_app_start(void)
{
    // 存档先读出来，标题页要用它显示「已见 / 最好成绩」。读不到就是首次开机，
    // 存档层已经把这种情况当正常路径处理（复位成空档并返回 OK），所以这里
    // 只需要处理真正的失败。
    const esp_err_t err = qpq_store_load(&s_progress);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "进度读取未成功（%s），按空档处理", esp_err_to_name(err));
    }

    bool audio = true;
    if (qpq_store_load_audio_enabled(&audio) == ESP_OK) {
        s_audio_enabled = audio;
    }
    qpq_player_set_muted(!s_audio_enabled);

    seed_session();
    s_page = PAGE_NONE;
    s_leave = NULL;
    s_screen = NULL;
    // 用 replace_page 而不是直接建页：这样「第一次进入」也走同一条路径，
    // 不会出现「首页特殊」这种只在一处存在的分支。
    qpq_app_goto_title();
}
