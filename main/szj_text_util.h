// main/szj_text_util.h —— 经文数据的访问与拼装(与 ESP-IDF/LVGL 无关,可做宿主测试)。
#pragma once

#include <stddef.h>

// 第 index 句经文;越界返回 NULL。
const char *szj_line(int index);

// 把第 stanza 课的 4 句拼成一个 12 字字符串写入 out。
// 返回写入的字节数(不含结尾 NUL);参数非法或容量不足返回 0。
size_t szj_stanza_text(int stanza, char *out, size_t capacity);

// 第 line 句所属的课号(0 基);越界返回 -1。
int szj_stanza_of_line(int line);

// 需要写入 strlen("人之初")*4 + 1 个字节。
#define SZJ_STANZA_TEXT_CAPACITY 64
