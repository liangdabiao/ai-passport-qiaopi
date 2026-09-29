// main/qpq_player.c —— 见 qpq_player.h。
//
// 结构：一个工作任务 + 一个命令队列 + 一个代次计数器 + 一份音效叠加状态。
//   - 队列：调用方只入队，绝不阻塞。
//   - 代次：每入队一个「抢占型」命令就自增；正在播的那一段在每个分块之间比对
//     代次，发现变了就立刻退出。这样「打断」不需要抢占，也不需要强制删除任务
//     （强制删除一个正握着 I2S 的任务是本仓库明确禁止的）。
//   - 叠加：音效不走队列播放，而是被设置成一份叠加状态，由正在写 PCM 的那条
//     路径在每个分块里混进去。这既保证了「只有一个写入者」，又让音效不会把
//     背景音乐切断。
#include "qpq_player.h"

#include <stdbool.h>
#include <string.h>

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "qpq_adpcm.h"
#include "qpq_audio.h"
#include "qpq_audio_blob.h"
#include "qpq_audio_index.h"
#include "qpq_volume.h"

static const char *TAG = "qpq_player";

// 一次解码并写入的样本数。512 个样本 = 1 KiB，约 32 ms 的音频；块越小打断越及时，
// 越大 I2S 欠载风险越小。32 ms 在「按键响应」与「播放稳定」之间够用。
#define QPQ_PLAYER_CHUNK_SAMPLES 512

// 三路音频在用户音量 100% 时的编解码器音量。配音要压过背景音乐，提示音在两者
// 之间。用户的音量档位按比例缩放它们（见 main/qpq_volume.c），所以调音量不会把
// 配音与音乐的平衡调歪 —— 这是把「音量」做成一个整体档位、而不是给每一路各开一个
// 设置的前提。
//
// 这三个值刻意写成 100/75/63 而不是「默认下要听到的」80/60/50：默认档位是 80%，
// 而 100/75/63 乘以 80% 正好是 80/60/50，也就是本应用在加入音量功能之前写死的三个
// 数。于是**默认档位下听感与之前逐位相同**，用户只有自己往上/往下调时才听到变化。
// tests/test_qpq_volume.c 把这个等式钉住了。
#define QPQ_PLAYER_VOICE_VOLUME QPQ_VOLUME_BASE_VOICE
#define QPQ_PLAYER_BGM_VOLUME QPQ_VOLUME_BASE_BGM
#define QPQ_PLAYER_TONE_VOLUME QPQ_VOLUME_BASE_TONE

// 方波音效的幅度。满幅是 32767，取 6000 约 18% —— 提示音不该盖过语音。
#define QPQ_TONE_AMPLITUDE 6000

#define QPQ_PLAYER_QUEUE_DEPTH 6
#define QPQ_PLAYER_TASK_STACK 4096
#define QPQ_PLAYER_TASK_PRIORITY 4

typedef enum {
    CMD_VOICE = 0,
    CMD_BGM,
    CMD_STOP_ALL,
    CMD_TONE,        // clip 字段携带 qpq_tone_t
    CMD_VOLUME,      // 用户改了音量档位，重新应用到音频设备
} qpq_player_command_kind_t;

typedef struct {
    qpq_player_command_kind_t kind;
    uint16_t clip;
} qpq_player_command_t;

// 音效：一串 (频率, 时长) 步，频率 0 表示结束。频率本身就是方波周期。
typedef struct {
    uint16_t hz;
    uint16_t ms;
} qpq_tone_step_t;

// 与网页版的合成音效对应：移动是极短的一跳，对错是往上／往下两个音，
// 一局结束是四音上行的终止式。音高偏低（o=5 附近），因为这台机器在床头用。
static const qpq_tone_step_t s_tones[QPQ_TONE_COUNT][5] = {
    [QPQ_TONE_MOVE]     = {{880, 25}, {0, 0}},
    [QPQ_TONE_ENTER]    = {{660, 45}, {990, 70}, {0, 0}},
    [QPQ_TONE_BACK]     = {{660, 45}, {440, 70}, {0, 0}},
    [QPQ_TONE_CORRECT]  = {{784, 60}, {1046, 70}, {1318, 130}, {0, 0}},
    [QPQ_TONE_WRONG]    = {{392, 90}, {294, 150}, {0, 0}},
    [QPQ_TONE_COMPLETE] = {{523, 80}, {659, 80}, {784, 80}, {1046, 190}},
};

static QueueHandle_t s_queue;
static TaskHandle_t s_task;

// 代次：抢占型命令会让它自增，正在播的那一段据此退出。
static volatile uint32_t s_generation;
// 应用是否希望有背景音乐。配音播完后据此决定要不要接回去。
static volatile bool s_bgm_requested;
static volatile bool s_voice_active;

// 用户音量档位。界面任务写、播放任务读；一个字节的读写在 RISC-V 上是原子的，
// 所以这里不加锁 —— 加锁反而会把一个「调音量」的动作变成可能阻塞按键回调的路径。
// 0 就是关，没有第二个静音标志。
static volatile uint8_t s_volume = QPQ_VOLUME_DEFAULT;
// 当前正在播的那一路的基准音量。档位变化时据此重算，所以「播放中调音量」立刻生效。
static uint8_t s_active_base = QPQ_VOLUME_BASE_BGM;

// 音效叠加状态。只由播放任务读写；界面通过队列请求，所以不必加锁。
typedef struct {
    const qpq_tone_step_t *steps;
    int step_index;
    uint32_t remaining_in_step;
    uint32_t phase;
    bool active;
} qpq_overlay_t;

static qpq_overlay_t s_overlay;

// 解码缓冲做成模块级静态：只有播放任务碰它，而且它比放进任务栈更省栈空间。
static int16_t s_chunk[QPQ_PLAYER_CHUNK_SAMPLES];

static qpq_audio_index_t s_index;
static bool s_audio_ready;

// 把用户档位乘到某一路的基准音量上。
static uint8_t scaled_volume(uint8_t base)
{
    return qpq_volume_scale(base, (uint8_t)s_volume);
}

// 应用某一路的音量。**只由播放任务调用** —— 所有触碰音频 codec 的动作都留在这一个
// 任务里，与「I2S 只有一个写入者」是同一条纪律：换音量本质是一次 I2C 寄存器写，
// 从界面任务直接写会和正在播放的任务撞在同一条 I2C 总线上。
static void apply_volume(uint8_t base)
{
    s_active_base = base;
    bsp_audio_set_volume(scaled_volume(base));
}

static void overlay_begin(qpq_tone_t tone)
{
    if (tone >= QPQ_TONE_COUNT) return;
    s_overlay.steps = s_tones[tone];
    s_overlay.step_index = 0;
    s_overlay.remaining_in_step = 0;
    s_overlay.phase = 0;
    s_overlay.active = s_overlay.steps[0].hz != 0;
}

// 把当前的音效混进 buf 的前 count 个样本里，并推进音效状态。
// 用饱和加法而不是直接相加：两路满幅相加会绕回成刺耳的爆音。
static void overlay_mix(int16_t *buf, uint32_t count)
{
    if (!s_overlay.active) return;

    for (uint32_t i = 0; i < count; i++) {
        if (s_overlay.remaining_in_step == 0) {
            const qpq_tone_step_t *step = &s_overlay.steps[s_overlay.step_index];
            if (step->hz == 0) {
                s_overlay.active = false;
                return;
            }
            s_overlay.remaining_in_step =
                (uint32_t)QPQ_AUDIO_SAMPLE_RATE * step->ms / 1000u;
            s_overlay.phase = 0;
        }

        const qpq_tone_step_t *step = &s_overlay.steps[s_overlay.step_index];
        const int16_t wave =
            (s_overlay.phase < (uint32_t)QPQ_AUDIO_SAMPLE_RATE / 2u)
                ? QPQ_TONE_AMPLITUDE
                : -QPQ_TONE_AMPLITUDE;
        s_overlay.phase += step->hz;
        if (s_overlay.phase >= (uint32_t)QPQ_AUDIO_SAMPLE_RATE) {
            s_overlay.phase -= (uint32_t)QPQ_AUDIO_SAMPLE_RATE;
        }

        int mixed = (int)buf[i] + (int)wave;
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        buf[i] = (int16_t)mixed;

        s_overlay.remaining_in_step--;
        if (s_overlay.remaining_in_step == 0) s_overlay.step_index++;
    }
}

// 把一块样本写进 I2S。音量为「关」时写零而不是跳过写入 —— 跳过会让循环空转，并让
// 播放位置与真实时间脱节，把音量调回来时会跳一下。
static bool write_chunk(uint32_t count)
{
    if (count == 0) return true;
    if (s_volume == QPQ_VOLUME_OFF) memset(s_chunk, 0, (size_t)count * sizeof(s_chunk[0]));
    return bsp_audio_write(s_chunk, (size_t)count * sizeof(s_chunk[0])) == ESP_OK;
}

// 播一个 ADPCM 片段。loop 为真时到末尾自动回到开头（背景音乐）。
// 返回 false 表示被新命令打断或写入失败。
static bool stream_clip(uint32_t generation, uint16_t clip, bool loop)
{
    if (!s_audio_ready || !qpq_audio_index_valid_clip(&s_index, clip)) return false;

    qpq_adpcm_stream_t stream;
    qpq_adpcm_stream_open(&stream, qpq_audio_index_clip(&s_index, clip),
                          qpq_audio_index_samples(&s_index, clip));

    for (;;) {
        if (generation != s_generation) return false;   // 被新命令打断

        const uint32_t got =
            qpq_adpcm_stream_read(&stream, s_chunk, QPQ_PLAYER_CHUNK_SAMPLES);
        if (got > 0) {
            overlay_mix(s_chunk, got);
            if (!write_chunk(got)) return false;
        } else if (!qpq_adpcm_stream_done(&stream)) {
            return false;   // 不该发生，防御
        }

        if (qpq_adpcm_stream_done(&stream)) {
            if (!loop) return true;
            qpq_adpcm_stream_rewind(&stream);
        }
    }
}

// 背景音乐：解一块、混一块音效、写一块，然后非阻塞看一眼命令。
// 返回 false 表示本轮结束（被打断或写入失败）。
static bool stream_bgm_chunk(uint32_t generation, qpq_adpcm_stream_t *stream)
{
    if (!s_audio_ready) return false;
    if (generation != s_generation) return false;

    const uint32_t got =
        qpq_adpcm_stream_read(stream, s_chunk, QPQ_PLAYER_CHUNK_SAMPLES);
    if (got > 0) {
        overlay_mix(s_chunk, got);
        if (!write_chunk(got)) return false;
    } else if (!qpq_adpcm_stream_done(stream)) {
        return false;
    }

    if (qpq_adpcm_stream_done(stream)) qpq_adpcm_stream_rewind(stream);
    return generation == s_generation;
}

// 播放一个音效（不与任何东西并行时用，例如标题页）。始终混入，保持与叠加路径
// 同一条出口。
static void play_tone_now(uint32_t generation, qpq_tone_t tone)
{
    overlay_begin(tone);
    while (s_overlay.active) {
        if (generation != s_generation) return;
        memset(s_chunk, 0, sizeof(s_chunk));
        overlay_mix(s_chunk, QPQ_PLAYER_CHUNK_SAMPLES);
        if (!write_chunk(QPQ_PLAYER_CHUNK_SAMPLES)) return;
    }
}

static void run_command(const qpq_player_command_t *command, uint32_t generation,
                        bool *bgm_running)
{
    *bgm_running = false;

    switch (command->kind) {
        case CMD_STOP_ALL:
            s_bgm_requested = false;
            s_voice_active = false;
            s_overlay.active = false;
            return;

        case CMD_BGM:
            if (!s_audio_ready) return;
            s_bgm_requested = true;
            s_voice_active = false;
            apply_volume(QPQ_PLAYER_BGM_VOLUME);
            *bgm_running = true;
            return;

        case CMD_VOICE:
            if (!s_audio_ready) return;
            apply_volume(QPQ_PLAYER_VOICE_VOLUME);
            s_voice_active = true;
            // 配音优先：这里只播一次，背景音乐在这段时间里让位。
            stream_clip(generation, command->clip, false);
            s_voice_active = false;
            if (generation != s_generation) return;   // 期间有更新的命令
            if (s_bgm_requested) {
                apply_volume(QPQ_PLAYER_BGM_VOLUME);
                *bgm_running = true;
            }
            return;

        default:
            return;
    }
}

static void player_task(void *arg)
{
    (void)arg;

    if (bsp_audio_set_format((uint32_t)QPQ_AUDIO_SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "音频格式设置失败，本次运行将没有声音");
        s_audio_ready = false;
    } else {
        s_audio_ready = true;
        apply_volume(QPQ_PLAYER_BGM_VOLUME);
    }

    bool bgm_running = false;
    uint32_t generation = s_generation;
    qpq_player_command_t command;
    qpq_adpcm_stream_t bgm_stream;

    for (;;) {
        if (bgm_running) {
            // 背景音乐：一次一块，块间非阻塞收命令。这样音效（叠加）与配音
            // （让位）都能在约 32ms 内生效，而不是等整首放完。
            generation = s_generation;
            if (!stream_bgm_chunk(generation, &bgm_stream)) {
                bgm_running = false;
            }
            while (xQueueReceive(s_queue, &command, 0) == pdTRUE) {
                generation = s_generation;
                if (command.kind == CMD_TONE) {
                    overlay_begin((qpq_tone_t)command.clip);
                    continue;      // 音效只设叠加状态，不打断音乐
                }
                if (command.kind == CMD_VOLUME) {
                    // 音量变化同样**不**能走 run_command：那条路径会把 bgm_running
                    // 置假，随后音乐会被从头重新打开 —— 用户每调一次音量，音乐就
                    // 从头开始一遍。这里就地改音量，位置不动。
                    apply_volume(s_active_base);
                    continue;
                }
                bgm_running = false;
                run_command(&command, generation, &bgm_running);
            }
            if (bgm_running) continue;

            // BGM 刚被停掉：若应用还想要音乐（例如只是被打断了一下），接着放。
            if (s_bgm_requested && s_audio_ready && !s_voice_active) {
                generation = s_generation;
                apply_volume(QPQ_PLAYER_BGM_VOLUME);
                qpq_adpcm_stream_open(
                    &bgm_stream,
                    qpq_audio_index_clip(&s_index, (uint16_t)QPQ_AUDIO_BGM_CLIP),
                    qpq_audio_index_samples(&s_index, (uint16_t)QPQ_AUDIO_BGM_CLIP));
                bgm_running = true;
            }
            continue;
        }

        if (xQueueReceive(s_queue, &command, portMAX_DELAY) != pdTRUE) continue;
        generation = s_generation;

        if (command.kind == CMD_VOLUME) {
            // 没有音乐、也没有配音在放：什么都不用重开，只把新音量写给 codec。
            apply_volume(s_active_base);
            continue;
        }

        if (command.kind == CMD_TONE) {
            // 没有音乐在放：音效自己占满出口。
            apply_volume(QPQ_PLAYER_TONE_VOLUME);
            play_tone_now(generation, (qpq_tone_t)command.clip);
            continue;
        }

        if (command.kind == CMD_BGM) {
            apply_volume(QPQ_PLAYER_BGM_VOLUME);
            qpq_adpcm_stream_open(
                &bgm_stream,
                qpq_audio_index_clip(&s_index, (uint16_t)QPQ_AUDIO_BGM_CLIP),
                qpq_audio_index_samples(&s_index, (uint16_t)QPQ_AUDIO_BGM_CLIP));
        }
        run_command(&command, generation, &bgm_running);
    }
}

esp_err_t qpq_player_start(void)
{
    if (s_task) return ESP_OK;

    const qpq_audio_status_t status =
        qpq_audio_index_open(&s_index, qpq_audio_blob(), qpq_audio_blob_size());
    if (status != QPQ_AUDIO_OK) {
        // 音频资源不合法不该让整个应用起不来：没有声音仍然可以答题。
        ESP_LOGE(TAG, "音频资源不可用（%s），本次运行将没有声音",
                 qpq_audio_status_name(status));
    }

    s_queue = xQueueCreate(QPQ_PLAYER_QUEUE_DEPTH, sizeof(qpq_player_command_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    s_generation = 1;
    s_bgm_requested = false;
    s_voice_active = false;
    s_overlay.active = false;

    if (xTaskCreate(player_task, "qpq_player", QPQ_PLAYER_TASK_STACK, NULL,
                    QPQ_PLAYER_TASK_PRIORITY, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void qpq_player_stop(void)
{
    s_generation++;                 // 让正在播的那一段尽快退出
    s_bgm_requested = false;
    s_overlay.active = false;
    if (s_queue) {
        xQueueReset(s_queue);
        vQueueDelete(s_queue);
        s_queue = NULL;
    }
    // 不强制删除任务：等它自己走完当前分块并阻塞在队列上。这里只放弃句柄，
    // 由调用方保证不再使用本模块。强制删除会打断正在进行的 I2S 写入。
    s_task = NULL;
    s_voice_active = false;
}

// 抢占型命令：自增代次，让正在播的那一段在下一个分块退出。
static bool enqueue(qpq_player_command_kind_t kind, uint16_t clip, bool preempt)
{
    if (!s_queue) return false;
    if (preempt) ++s_generation;
    qpq_player_command_t command = {.kind = kind, .clip = clip};
    // 0 超时：绝不阻塞调用方（可能是按键回调）。
    return xQueueSend(s_queue, &command, 0) == pdTRUE;
}

void qpq_player_play_voice(uint16_t clip)
{
    if (!s_audio_ready) return;
    if (clip >= (uint16_t)QPQ_AUDIO_NARRATION_COUNT) return;
    // 音量关掉时直接不播（解码出来也是零，不如省掉一次抢占）。这里刻意**不**动
    // 代次：音量是全局档位，不该顺手把背景音乐也打断 —— 那会让「把音量调回来之后
    // 音乐回不来」成为一个很难查的现象。
    if (s_volume == QPQ_VOLUME_OFF) return;
    enqueue(CMD_VOICE, clip, true);
}

void qpq_player_start_bgm(void)
{
    if (!s_audio_ready) return;
    if (QPQ_AUDIO_BGM_CLIP < 0) return;
    enqueue(CMD_BGM, (uint16_t)QPQ_AUDIO_BGM_CLIP, true);
}

void qpq_player_stop_all(void)
{
    enqueue(CMD_STOP_ALL, 0, true);
}

void qpq_player_play_tone(qpq_tone_t tone)
{
    if (!s_audio_ready) return;
    if (s_volume == QPQ_VOLUME_OFF) return;
    // 音效刻意**不**抢占：它要被叠加进正在播放的音频里，而不是把音乐切断。
    // 代价是队列满时会丢一个提示音 —— 提示音丢一个无妨，音乐断一下很刺耳。
    enqueue(CMD_TONE, (uint16_t)tone, false);
}

bool qpq_player_voice_active(void)
{
    return s_voice_active;
}

bool qpq_player_ready(void)
{
    return s_audio_ready;
}

void qpq_player_set_volume(uint8_t percent)
{
    const uint8_t clamped = percent > QPQ_VOLUME_MAX ? QPQ_VOLUME_MAX : percent;
    if (s_volume == clamped) return;
    s_volume = clamped;
    // 只改档位，然后通知播放任务去应用。正在播的音频会继续往下走（关档写零样本），
    // 所以时序不乱、位置不跳，也不需要重建播放状态。
    //
    // 非抢占：调音量不该打断任何东西 —— 尤其是背景音乐，从头再来一遍比音量晚
    // 32ms 生效刺耳得多。队列满时丢掉这次通知不影响正确性：下一次播放开始时会
    // 按新档位重算音量。
    enqueue(CMD_VOLUME, 0, false);
}

uint8_t qpq_player_volume(void)
{
    return s_volume;
}
