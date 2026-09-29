// main/main.c —— 侨批 · 填字问答的入口。
//
// 只做三件事：把 BSP 外设点起来、把按键事件搬进队列、把界面交给 qpq_app 管。
// 页面、答题状态机、折行、音频都在 qpq_* 模块里，这里刻意保持薄。
//
// 硬件只有三个键（上/下/确定，共用一个 ADC 分压引脚），所以全局只用一套语义：
//   上/下 短按   在当前页面里移动（选候选 / 选菜单项）
//   确定  短按   往下走一步（确认作答 / 下一题 / 进入某项）
//   确定  长按   退回上一层；在答题中按等于放弃本局
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

#include "qpq_app.h"
#include "qpq_player.h"
#include "qpq_store.h"

static const char *TAG = "qiaopi";

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

// 按键回调运行在共享的 esp_timer 任务上：只做一次非阻塞入队，立刻返回。
// 任何慢操作（提示音、NVS、LVGL）都不允许出现在这里。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = event };
    (void)xQueueSend(s_input_queue, &input, 0);
}

static bool translate(const input_event_t *input, qpq_key_t *out)
{
    if (input->event == BSP_BTN_LONG) {
        if (input->btn == BSP_BTN_OK) {
            *out = QPQ_KEY_BACK;
            return true;
        }
        return false;
    }
    if (input->event != BSP_BTN_CLICK) return false;

    switch (input->btn) {
        case BSP_BTN_UP:   *out = QPQ_KEY_UP;   return true;
        case BSP_BTN_DOWN: *out = QPQ_KEY_DOWN; return true;
        case BSP_BTN_OK:   *out = QPQ_KEY_OK;   return true;
        default:           return false;
    }
}

static void input_task(void *argument)
{
    (void)argument;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) != pdTRUE) continue;
        qpq_key_t key;
        if (translate(&input, &key)) qpq_app_key(key);
    }
}

static esp_err_t input_start(void)
{
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;

    if (xTaskCreate(input_task, "qpq_input", INPUT_TASK_STACK, NULL,
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
    ESP_LOGI(TAG, "侨批 · 填字问答 启动");

    const esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "休眠唤醒原因: %d", wakeup);
    }

    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "I2C 初始化失败: %s", esp_err_to_name(err));
    (void)bsp_i2c_scan();

    // 屏幕是这个应用的主要出口，点不亮就没得玩 —— 打清楚日志后停在这里，
    // 不做「串口菜单」之类的降级（那是另一套要长期维护的界面）。
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败。检查 SPI 接线"
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    // 进度存档。存不了也照常能玩，只是重启后从头开始。
    err = qpq_store_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "进度存储不可用: %s", esp_err_to_name(err));

    // 单项外设失败都不阻塞：没有声音仍然可以答题，电量读不到就显示 "--"。
    if (bsp_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "音频编解码初始化失败，将以静音方式运行");
    } else if (qpq_player_start() != ESP_OK) {
        ESP_LOGW(TAG, "播放服务启动失败，将以静音方式运行");
    }
    if (bsp_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "电量计不可用，顶栏会显示 --");
    }

    const esp_err_t input_err = input_start();
    if (input_err != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败: %s", esp_err_to_name(input_err));
    }

    if (bsp_lvgl_lock(1000)) {
        qpq_app_start();
        bsp_lvgl_unlock();
        // 界面就绪之后才放行按键，避免开机瞬间的按键落到还不存在的页面上。
        s_input_ready = true;
    }

    ESP_LOGI(TAG, "就绪:显示=OK 按键=%s 音频=%s 静音=%s",
             input_err == ESP_OK ? "OK" : "FAIL",
             qpq_player_ready() ? "OK" : "FAIL",
             qpq_player_muted() ? "开" : "关");
}
