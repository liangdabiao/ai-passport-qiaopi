// main/qpq_content.h —— 题库的只读访问。
//
// 表本身由 tools/qiaopi/gen_content.py 从 tools/qiaopi/bank.txt 生成；这里只提供
// 带边界检查的访问与几处派生计算，好让界面层与宿主测试都不必自己数下标。
//
// 不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "qpq_text.h"

// 题目总数与逐题访问。
uint16_t qpq_question_count(void);
const qpq_question_t *qpq_question_at(uint16_t index);

// 分类表与选项标签。
uint16_t qpq_category_count(void);
const char *qpq_category_at(uint16_t index);
const char *qpq_answer_slot(uint8_t index);

// 结算评级：取第一个满足「正确率 >= 该档下限」的档位。percent 为负或超过 100
// 时按夹取后的值处理，不返回 NULL。
const qpq_rank_t *qpq_rank_for_percent(int percent);

// 题目句按填空标记拆成前后两段。填空位在设备上是画出来的槽位，所以界面需要
// 分开渲染前后文本。
typedef struct {
    const char *prefix;       // 填空位之前的文本（指向表里的字符串，不复制）
    uint16_t prefix_bytes;    // prefix 的字节数
    const char *suffix;       // 填空位之后的文本
} qpq_sentence_split_t;

// 拆分成功返回 true。填空标记出现次数不是恰好一次时返回 false —— 那说明生成器
// 漏了校验，属于必须显式暴露的缺陷，不能靠猜。
bool qpq_sentence_split(const qpq_question_t *question, qpq_sentence_split_t *out);

// 填空位被正确答案填上之后的字符数。设备版式按这个长度算，不是按源码里带
// 下划线的长度算（"____" 是 4 个字符，正确答案是 2..4 个）。找不到标记时返回 0。
uint16_t qpq_filled_length(const qpq_question_t *question);

// 答题页显示的句子：把填空标记换成一个等宽的空白槽位（四个全角下划线）。
// **字符数与源句完全相同** —— 答题页在选答案之前只能显示槽位，所以它的版式
// 必须与源句一样放得下；换成别的写法就会悄悄多出或少掉位置。
// 返回写入的字节数；容量不足或句子不合法时返回 0，并把 out 置为空串。
uint16_t qpq_sentence_slot(const qpq_question_t *question, char *out, size_t capacity);

// 判卷页显示的句子：把填空标记换成正确答案。返回写入的字节数；容量不足或句子
// 不合法时返回 0，并把 out 置为空串。
uint16_t qpq_sentence_filled(const qpq_question_t *question, char *out, size_t capacity);
