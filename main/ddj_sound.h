// main/ddj_sound.h —— 应用提示音。全部由芯片实时合成，不占用 Flash 音频素材。
//
// 这里刻意没有「答对 / 答错」这类音：这个应用不判分。声音只做两件事 ——
// 告诉你按键收到了，以及告诉你这一章记下了。
#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    DDJ_SOUND_MOVE = 0,  // 光标移动 / 翻一屏
    DDJ_SOUND_ENTER,     // 进入下一层
    DDJ_SOUND_BACK,      // 退回上一层
    DDJ_SOUND_SEAL,      // 参究表态已记下
    DDJ_SOUND_BLOCK,     // 这一步现在走不了（空页面之类），响一声拒绝
} ddj_sound_t;

// 启动播放器任务。音频 codec 不可用时返回错误，应用继续以静音方式运行。
esp_err_t ddj_sound_init(void);

// 播放一个提示音。未初始化、已静音或队列满时静默忽略；可从任意上下文调用（非阻塞）。
void ddj_sound_play(ddj_sound_t sound);

// 静音开关。关闭后 ddj_sound_play 直接返回，不再占用播放队列。
void ddj_sound_set_enabled(bool enabled);
bool ddj_sound_enabled(void);

bool ddj_sound_ready(void);
