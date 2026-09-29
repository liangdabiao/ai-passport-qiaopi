// main/szj_sound.h —— 应用音效。全部由芯片实时合成,不占用 Flash 音频素材。
#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    SZJ_SOUND_MOVE = 0,     // 光标移动
    SZJ_SOUND_ENTER,        // 进入某个页面
    SZJ_SOUND_BACK,         // 返回
    SZJ_SOUND_CORRECT,      // 答对
    SZJ_SOUND_WRONG,        // 答错
    SZJ_SOUND_LESSON_DONE,  // 一课完成
} szj_sound_t;

// 启动播放器任务。音频 codec 不可用时返回错误,应用继续以静音方式运行。
esp_err_t szj_sound_init(void);

// 播放一个音效。未初始化、已静音或队列满时静默忽略;可从任意上下文调用(非阻塞)。
void szj_sound_play(szj_sound_t sound);

// 静音开关。关闭后 szj_sound_play 直接返回,不再占用播放队列。
void szj_sound_set_enabled(bool enabled);
bool szj_sound_enabled(void);

bool szj_sound_ready(void);
