// main/qpq_volume.c —— 见 qpq_volume.h。
#include "qpq_volume.h"

#include <stdio.h>
#include <string.h>

const uint8_t qpq_volume_steps[QPQ_VOLUME_STEP_COUNT] = {
    QPQ_VOLUME_OFF, 20, 40, 60, QPQ_VOLUME_DEFAULT, QPQ_VOLUME_MAX,
};

// 默认档位必须在档位表里，否则「出厂默认」会是一个用户永远调不回来的值。
_Static_assert(QPQ_VOLUME_DEFAULT % 20 == 0, "默认档位必须落在 20% 的格点上");
_Static_assert(QPQ_VOLUME_DEFAULT < QPQ_VOLUME_MAX, "默认档位不能是满量程，否则用户没法再调大");
_Static_assert((int)QPQ_VOLUME_DEFAULT / 20 < QPQ_VOLUME_STEP_COUNT, "默认档位越出档位表");
_Static_assert(QPQ_VOLUME_MAX == 100, "缩放按百分比算，最大值必须是 100");

uint8_t qpq_volume_scale(uint8_t base, uint8_t level)
{
    // 先转 32 位再乘：base 最大 100、level 最大 255 时 8 位乘法会溢出。
    // 截断而不是四舍五入 —— 三个基准值在 80% 下都正好整除（100/75/63 -> 80/60/50），
    // 不需要靠舍入把「默认听感与以前一致」凑出来。
    return (uint8_t)(((uint32_t)base * (uint32_t)level) / 100u);
}

uint8_t qpq_volume_next(uint8_t level)
{
    for (int index = 0; index < QPQ_VOLUME_STEP_COUNT; index++) {
        if (qpq_volume_steps[index] == level) {
            return qpq_volume_steps[(index + 1) % QPQ_VOLUME_STEP_COUNT];
        }
    }
    // 不认识的档位：回到关。返回一个「表里没有的值」会让下一次调用也找不到，
    // 于是用户再也调不动音量 —— 那才是真的坏掉。
    return qpq_volume_steps[0];
}

uint16_t qpq_volume_text(uint8_t level, char *out, size_t capacity)
{
    if (!out || capacity == 0) return 0;
    out[0] = '\0';

    if (level == QPQ_VOLUME_OFF) {
        // 「关」是一个汉字，UTF-8 占 3 字节。容量不足时整体失败，不留半截。
        static const char k_off[] = "关";
        if (sizeof(k_off) > capacity) return 0;
        memcpy(out, k_off, sizeof(k_off));
        return (uint16_t)(sizeof(k_off) - 1u);
    }

    char buffer[8];
    const int written = snprintf(buffer, sizeof(buffer), "%u%%", (unsigned)level);
    if (written <= 0 || (size_t)written + 1u > capacity) return 0;
    memcpy(out, buffer, (size_t)written + 1u);
    return (uint16_t)written;
}
