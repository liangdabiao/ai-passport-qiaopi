// main/ddj_wrap.c —— 见 ddj_wrap.h。
#include "ddj_wrap.h"

#include <stdbool.h>

/* UTF-8 下一个字符占几个字节。非法首字节按 1 字节走，不在这里做校验 ——
 * 内容由生成器保证，这里只要不越界读。 */
static int utf8_length(unsigned char lead)
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

size_t ddj_wrap_utf8(const char *text, int chars_per_line, char *out, size_t capacity)
{
    if (!out || capacity == 0) return 0;
    out[0] = '\0';
    if (!text) return 0;
    if (chars_per_line < 1) return 0;

    size_t written = 0;
    int column = 0;
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
        const int length = utf8_length((unsigned char)*at);
        const bool closing = is_closing_punctuation(utf8_codepoint(at, length));

        // 提前一字换行：正要写这一行的最后一个字时，看一眼它后面那个字。
        // 如果后面是收尾标点，就在这里换行，把标点放到下一行去 —— 下一行以
        // 这个字开头，仍然不是标点开头的行。
        // column >= 1 保证换行后当前行不为空；收尾标点自己不做触发（否则
        // 「，。」这种连续标点会被拆到两行，行首反而还是标点）。
        if (column + 1 == chars_per_line && column >= 1 && !closing) {
            const char *next = at + length;
            if (*next) {
                const int next_length = utf8_length((unsigned char)*next);
                if (is_closing_punctuation(utf8_codepoint(next, next_length))) {
                    PUT_BYTE('\n');
                    column = 0;
                }
            }
        }

        if (column == chars_per_line) {
            PUT_BYTE('\n');
            column = 0;
        }

        for (int i = 0; i < length; i++) PUT_BYTE(at[i]);
        at += length;
        column++;
    }

#undef PUT_BYTE

    if (written + 1u > capacity) {
        out[0] = '\0';
        return 0;
    }
    out[written] = '\0';
    return written;
}
