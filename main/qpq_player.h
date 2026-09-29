// main/qpq_player.h —— PCM 播放服务：方言配音、背景音乐与界面音效。
//
// 设备上原本只有 RTTTL 音效合成（rtttl_player），没有任何「播放一段录音」的能力。
// 本模块把两件事合成一件：**I2S 出口只能有一个写入者**。两个任务各写各的，结果是
// 两股 PCM 交错插进同一个 DMA 缓冲 —— 听起来像噪声，而且很难查。所以这里既放
// 录音，也放音效。
//
// 线程约定照抄 rttrl_player：播放全部在播放器自己的任务里进行，调用方（含按键
// 回调、生命周期任务）只入队，绝不阻塞。
//
// 两处有意与网页版不同，都已在应用档案里记着：
//   1. 网页版把方言配音**叠在**背景音乐上（两个 Audio 对象并行）。设备只有一个
//      PCM 出口，叠播要混音，而且在一块 40mm 的喇叭上人声底下压着音乐只会互相
//      糊掉。所以播配音时让出背景音乐，播完接回。
//   2. 界面音效**叠加**在背景音乐上（网页版也是这个行为），因为它们只有几十
//      毫秒，让位反而会让音乐一顿一顿。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

// 界面音效。与网页版的四个 SFX（click / correct / wrong / complete）对应，
// 另加一个「返回」与一个「移动」，让三个键在菜单里的操作有区分。
typedef enum {
    QPQ_TONE_MOVE = 0,   // 光标移动
    QPQ_TONE_ENTER,      // 确认、进入
    QPQ_TONE_BACK,       // 返回
    QPQ_TONE_CORRECT,    // 答对
    QPQ_TONE_WRONG,      // 答错
    QPQ_TONE_COMPLETE,   // 一局结束
    QPQ_TONE_COUNT,
} qpq_tone_t;

// 启动播放任务。音频 codec 不可用时返回错误，应用继续以静音方式运行。
esp_err_t qpq_player_start(void);

// 停止任务并释放状态（生命周期用）。调用后可以再次 start。
void qpq_player_stop(void);

// 播放一道题的方言配音（片段号 = 题目下标）。会先让出背景音乐，播完自动接回。
// 传入越界片段号时什么都不做。
void qpq_player_play_voice(uint16_t clip);

// 开始循环播放背景音乐。重复调用是幂等的。
void qpq_player_start_bgm(void);

// 停掉一切：配音与背景音乐，并且取消「希望有背景音乐」的状态。界面音效不受影响 ——
// 它是即时的，不需要被「停止」。
void qpq_player_stop_all(void);

// 播一次界面音效。会与背景音乐叠加，不会打断它。
void qpq_player_play_tone(qpq_tone_t tone);

// 是否正在播配音（界面用来决定要不要显示喇叭图标）。
bool qpq_player_voice_active(void);

// 音频资源是否可用。blob 打不开（刷了半截固件、生成物版本不符）时为假，
// 这时所有播放请求都会被安静地忽略 —— 应用照常能答题。
bool qpq_player_ready(void);

// 用户音量档位（百分比，0..100）。0 就是关 —— 所以这里没有单独的静音开关：
// 三个键不值得两个设置项，而且「关」本来就是音量调到最小这一个动作。
//
// 档位表与缩放算术在 main/qpq_volume.c（纯逻辑、宿主可测），这里只负责把它
// 应用到音频设备上。传入越界值（>100）会被夹到 100。
void qpq_player_set_volume(uint8_t percent);
uint8_t qpq_player_volume(void);
