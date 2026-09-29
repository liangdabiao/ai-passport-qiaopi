// main/qpq_wrap.c —— 见 qpq_wrap.h。移植自本仓库前一个应用（三字经 / 道德经日课）
// 的同名模块，算法未改；这里刻意不依赖 ESP-IDF 与 LVGL，宿主测试直接编译它。
#include "qpq_wrap.h"

#include <stdbool.h>

/* UTF-8 下一个字符占几个字节。非法首字节按 1 字节走，不在这里做校验 ——
 * 内容由生成器保证，这里只要不越界读。 */
static int utf8_sequence_length(unsigned char lead)
{
    if (lead < 0x80u) return 1;
    if ((lead & 0xE0u) == 0xC0u) return 2;
    if ((lead & 0xF0u) == 0xE0u) return 3;
    if ((lead & 0xF8u) == 0xF0u) return 4;
    return 1;
}

static unsigned int utf8_codepoint(const char *at, int length)
{
    const unsigned char *bytes = (const unsigned char *)at;
    switch (length) {
        case 2:
            return ((unsigned int)(bytes[0] & 0x1Fu) << 6) |
                   (unsigned int)(bytes[1] & 0x3Fu);
        case 3:
            return ((unsigned int)(bytes[0] & 0x0Fu) << 12) |
                   ((unsigned int)(bytes[1] & 0x3Fu) << 6) |
                   (unsigned int)(bytes[2] & 0x3Fu);
        case 4:
            return ((unsigned int)(bytes[0] & 0x07u) << 18) |
                   ((unsigned int)(bytes[1] & 0x3Fu) << 12) |
                   ((unsigned int)(bytes[2] & 0x3Fu) << 6) |
                   (unsigned int)(bytes[3] & 0x3Fu);
        default:
            return bytes[0];
    }
}

/* 不可以出现在行首的收尾标点。 */
static bool is_closing_punctuation(unsigned int code)
{
    switch (code) {
        case 0xFF0Cu: /* ， */
        case 0x3002u: /* 。 */
        case 0xFF01u: /* ！ */
        case 0xFF1Fu: /* ？ */
        case 0x3001u: /* 、 */
        case 0xFF1Bu: /* ； */
        case 0xFF1Au: /* ： */
        case 0xFF09u: /* ） */
        case 0x300Du: /* 」 */
        case 0x300Fu: /* 』 */
        case 0x300Bu: /* 》 */
        case 0x3011u: /* 】 */
        case 0x2026u: /* … */
        case 0x2014u: /* — */
            return true;
        default:
            return false;
    }
}

unsigned int qpq_utf8_length(const char *text)
{
    if (!text) return 0;
    unsigned int count = 0;
    const char *at = text;
    while (*at) {
        at += utf8_sequence_length((unsigned char)*at);
        count++;
    }
    return count;
}

unsigned int qpq_utf8_length_prefix(const char *text, size_t max_bytes)
{
    if (!text) return 0;
    unsigned int count = 0;
    size_t consumed = 0;
    const char *at = text;
    while (*at && consumed < max_bytes) {
        const int length = utf8_sequence_length((unsigned char)*at);
        if (consumed + (size_t)length > max_bytes) break;
        at += length;
        consumed += (size_t)length;
        count++;
    }
    return count;
}

size_t qpq_wrap_utf8(const char *text, int chars_per_line, char *out, size_t capacity)
{
    if (!out || capacity == 0) return 0;
    out[0] = '\0';
    if (!text) return 0;
    if (chars_per_line < 1 || chars_per_line > QPQ_WRAP_MAX_LINE) return 0;

    size_t written = 0;
    const char *at = text;

    // 每写一个字节都留出结尾 NUL 的位置：容量不足时整体失败，绝不留半截字符串。
#define PUT_BYTE(byte)                                                             \
    do {                                                                           \
        if (written + 2u > capacity) {                                             \
            out[0] = '\0';                                                         \
            return 0;                                                              \
        }                                                                          \
        out[written++] = (byte);                                                   \
    } while (0)

    while (*at) {
        // 先探明这一行的候选字符：多收一个，用来判断「下一行会不会以标点开头」。
        const char *starts[QPQ_WRAP_MAX_LINE + 1];
        int lengths[QPQ_WRAP_MAX_LINE + 1];
        int collected = 0;
        const char *probe = at;
        while (*probe && collected <= chars_per_line) {
            const int length = utf8_sequence_length((unsigned char)*probe);
            starts[collected] = probe;
            lengths[collected] = length;
            probe += length;
            collected++;
        }

        int take;
        if (collected <= chars_per_line) {
            // 剩下的字一行放得下，不必再断点。
            take = collected;
        } else {
            // 后面还有字，第 chars_per_line 个字会成为下一行的开头。从最宽处
            // 往回收，直到下一行的首字不是收尾标点；退到 1 为止（此时无法再退，
            // 该情形已在 qpq_wrap.h 里说明）。
            take = chars_per_line;
            while (take > 1 &&
                   is_closing_punctuation(utf8_codepoint(starts[take], lengths[take]))) {
                take--;
            }
        }

        for (int index = 0; index < take; index++) {
            for (int byte = 0; byte < lengths[index]; byte++) {
                PUT_BYTE(starts[index][byte]);
            }
        }
        at = starts[take - 1] + lengths[take - 1];
        if (*at) PUT_BYTE('\n');
    }

#undef PUT_BYTE

    if (written + 1u > capacity) {
        out[0] = '\0';
        return 0;
    }
    out[written] = '\0';
    return written;
}
