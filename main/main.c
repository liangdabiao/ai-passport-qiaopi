// main/main.c —— 三字经儿童学习游戏的入口。
//
// 只做三件事:把 BSP 外设点起来、把按键事件搬进队列、把界面交给 szj_app 管。
// 页面、状态机、动画都在 szj_* 模块里,这里刻意保持薄。
//
// 硬件只有三个键(上/下/确定,共用一个 ADC 分压引脚),所以全局只用一套语义:
//   上/下 短按   在当前页面里移动光标
//   确定  短按   确认(进入 / 作答 / 切换)
//   确定  长按   返回上一级
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "szj_app.h"
#include "szj_sound.h"
#include "szj_store.h"

static const char *TAG = "sanzijing";

#define INPUT_QUEUE_DEPTH 8
#define INPUT_TASK_STACK  4096
#define INPUT_TASK_PRIO   5

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static TaskHandle_t s_input_task;
static volatile bool s_input_ready;

// 按键回调运行在共享的 esp_timer 任务上:只做一次非阻塞入队,立刻返回。
// 任何慢操作(音效、NVS、LVGL)都不允许出现在这里。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = event };
    (void)xQueueSend(s_input_queue, &input, 0);
}

static bool translate(const input_event_t *input, szj_key_t *out)
{
    if (input->event == BSP_BTN_LONG) {
        if (input->btn == BSP_BTN_OK) {
            *out = SZJ_KEY_BACK;
            return true;
        }
        return false;
    }
    if (input->event != BSP_BTN_CLICK) return false;

    switch (input->btn) {
        case BSP_BTN_UP:   *out = SZJ_KEY_UP;   return true;
        case BSP_BTN_DOWN: *out = SZJ_KEY_DOWN; return true;
        case BSP_BTN_OK:   *out = SZJ_KEY_OK;   return true;
        default:           return false;
    }
}

static void input_task(void *argument)
{
    (void)argument;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) != pdTRUE) continue;
        szj_key_t key;
        if (translate(&input, &key)) szj_app_key(key);
    }
}

static esp_err_t input_start(void)
{
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;

    if (xTaskCreate(input_task, "szj_input", INPUT_TASK_STACK, NULL,
                    INPUT_TASK_PRIO, &s_input_task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    const esp_err_t err = bsp_button_init(on_key, NULL);
    if (err != ESP_OK) {
        vTaskDelete(s_input_task);
        s_input_task = NULL;
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return err;
    }
    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "三字经儿童学习游戏 启动");

    const esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "休眠唤醒原因: %d", wakeup);
    }

    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "I2C 初始化失败: %s", esp_err_to_name(err));
    (void)bsp_i2c_scan();

    // 屏幕是这个应用的唯一出口,点不亮就没得玩 —— 打清楚日志后停在这里,
    // 不做"串口菜单"之类的降级(那是另一套要长期维护的界面)。
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败。检查 SPI 接线"
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    // 学习进度与设置。存不了也让游戏照常能玩,只是重启后从头开始。
    err = szj_store_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "进度存储不可用: %s", esp_err_to_name(err));
    szj_sound_set_enabled(szj_store_audio_enabled());

    // 单项外设失败都不阻塞:音效没了还能静音玩,电量读不到就显示 "--"。
    if (bsp_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "音频编解码初始化失败,将以静音方式运行");
    } else if (szj_sound_init() != ESP_OK) {
        ESP_LOGW(TAG, "音效播放器启动失败,将以静音方式运行");
    }
    if (bsp_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "电量计不可用,顶栏会显示 --");
    }

    const esp_err_t input_err = input_start();
    if (input_err != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败: %s", esp_err_to_name(input_err));
    }

    if (bsp_lvgl_lock(1000)) {
        szj_app_start();
        bsp_lvgl_unlock();
        // 界面就绪之后才放行按键,避免开机瞬间的按键落到还不存在的页面上。
        s_input_ready = true;
    }

    ESP_LOGI(TAG, "就绪:显示=OK 按键=%s 音效=%s",
             input_err == ESP_OK ? "OK" : "FAIL",
             szj_sound_ready() ? "OK" : "静音");
}
