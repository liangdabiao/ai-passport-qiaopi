// tests/test_qpq_volume.c —— 音量档位表与缩放算术。
//
// 这个测试存在的唯一理由是钉住一句话：**默认档位下的听感与加入音量功能之前完全
// 一致**。那句话如果只是注释，下一个人把基准值从 63 改成 64 就悄悄变了，谁也不会
// 发现 —— 除非有人拿着分贝计去听。这里把它变成断言。
//
// 还有一条同样重要的：0 档必须让每一路都真的归零。音量功能最常见的缺陷就是
// 「调到关还有声音」，而那是用户在床头最会介意的一种。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "qpq_volume.h"

// 加音量功能之前写死在播放器里的三个值。它们就是这套档位必须复现的目标。
#define LEGACY_VOICE 80
#define LEGACY_TONE  60
#define LEGACY_BGM   50

static void test_default_reproduces_the_legacy_fixed_volumes(void)
{
    // 这一条是整套档位的存在理由：默认 80% 必须把三个基准值缩放回原来的三个数。
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_VOICE, QPQ_VOLUME_DEFAULT) == LEGACY_VOICE);
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_TONE, QPQ_VOLUME_DEFAULT) == LEGACY_TONE);
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_BGM, QPQ_VOLUME_DEFAULT) == LEGACY_BGM);
    puts("ok  默认档位复现旧的固定音量（100/75/63 x 80% = 80/60/50）");
}

static void test_full_scale_is_louder_than_the_legacy_volumes(void)
{
    // 往上调必须真的有空间，否则「能调音量」是假的 —— 用户能做的只有变小。
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_VOICE, QPQ_VOLUME_MAX) > LEGACY_VOICE);
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_TONE, QPQ_VOLUME_MAX) > LEGACY_TONE);
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_BGM, QPQ_VOLUME_MAX) > LEGACY_BGM);
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_VOICE, QPQ_VOLUME_MAX) <= 100);
    puts("ok  满档比旧固定值更响，且不超过 100");
}

static void test_off_is_silent_on_every_path(void)
{
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_VOICE, QPQ_VOLUME_OFF) == 0);
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_TONE, QPQ_VOLUME_OFF) == 0);
    assert(qpq_volume_scale(QPQ_VOLUME_BASE_BGM, QPQ_VOLUME_OFF) == 0);
    // 任何一路、任何 base，0 档都必须是 0。
    for (int base = 0; base <= 100; base++) {
        assert(qpq_volume_scale((uint8_t)base, QPQ_VOLUME_OFF) == 0);
    }
    puts("ok  0 档在每一路上都归零");
}

static void test_scaling_is_monotonic_and_bounded(void)
{
    for (int base = 0; base <= 100; base++) {
        uint8_t previous = 0;
        for (int index = 0; index < QPQ_VOLUME_STEP_COUNT; index++) {
            const uint8_t scaled =
                qpq_volume_scale((uint8_t)base, qpq_volume_steps[index]);
            assert(scaled >= previous);          // 档位升高，音量不降
            assert(scaled <= (uint8_t)base);     // 缩放不会超过基准值
            previous = scaled;
        }
    }
    // 乘法的中间值必须走 32 位：8 位下 100 x 100 会绕回。
    assert(qpq_volume_scale(100, 100) == 100);
    assert(qpq_volume_scale(255, 100) == 255);
    puts("ok  缩放单调、有上界、且不发生 8 位溢出");
}

static void test_step_table_shape(void)
{
    assert(qpq_volume_steps[0] == QPQ_VOLUME_OFF);
    assert(qpq_volume_steps[QPQ_VOLUME_STEP_COUNT - 1] == QPQ_VOLUME_MAX);

    int found_default = 0;
    for (int index = 0; index < QPQ_VOLUME_STEP_COUNT; index++) {
        if (qpq_volume_steps[index] == QPQ_VOLUME_DEFAULT) found_default = 1;
        if (index > 0) assert(qpq_volume_steps[index] > qpq_volume_steps[index - 1]);
    }
    // 默认档位不在表里，用户调到别处之后就再也回不到出厂档位。
    assert(found_default);
    puts("ok  档位表严格递增，且包含默认档位");
}

static void test_next_cycles_and_recovers_from_unknown_values(void)
{
    uint8_t level = QPQ_VOLUME_OFF;
    // 从关开始，一路按到底必须回绕到关 —— 不能出现「卡在满档、再也关不掉」。
    for (int index = 0; index < QPQ_VOLUME_STEP_COUNT; index++) {
        level = qpq_volume_next(level);
    }
    assert(level == QPQ_VOLUME_OFF);

    assert(qpq_volume_next(QPQ_VOLUME_MAX) == QPQ_VOLUME_OFF);
    assert(qpq_volume_next(QPQ_VOLUME_DEFAULT) == QPQ_VOLUME_MAX);

    // 存档被改坏或来自旧版本时，档位可能不在表里。这时必须回到一个合法档位，
    // 否则下一次 qpq_volume_next 也找不到它，音量就永久卡死。
    assert(qpq_volume_next(37) == qpq_volume_steps[0]);
    assert(qpq_volume_next(255) == qpq_volume_steps[0]);
    puts("ok  档位循环可回绕，未知档位回到合法档位");
}

static void test_text_and_capacity_contract(void)
{
    char buffer[16];

    assert(qpq_volume_text(QPQ_VOLUME_OFF, buffer, sizeof(buffer)) == 3);  // 「关」3 字节
    assert(strcmp(buffer, "关") == 0);

    assert(qpq_volume_text(QPQ_VOLUME_DEFAULT, buffer, sizeof(buffer)) == 3);
    assert(strcmp(buffer, "80%") == 0);

    assert(qpq_volume_text(QPQ_VOLUME_MAX, buffer, sizeof(buffer)) == 4);
    assert(strcmp(buffer, "100%") == 0);

    // 容量不足：必须整体失败并留下空串，不能留半截（半个汉字会渲染成空白框）。
    assert(qpq_volume_text(QPQ_VOLUME_OFF, buffer, 3) == 0);
    assert(buffer[0] == '\0');
    assert(qpq_volume_text(QPQ_VOLUME_MAX, buffer, 4) == 0);
    assert(buffer[0] == '\0');
    assert(qpq_volume_text(QPQ_VOLUME_MAX, buffer, 5) == 4);
    assert(strcmp(buffer, "100%") == 0);

    // 空指针与零容量：只保证返回值，不保证写任何字节。
    assert(qpq_volume_text(QPQ_VOLUME_DEFAULT, NULL, sizeof(buffer)) == 0);
    assert(qpq_volume_text(QPQ_VOLUME_DEFAULT, buffer, 0) == 0);
    puts("ok  档位文本与容量不足时的整体失败");
}

int main(void)
{
    test_default_reproduces_the_legacy_fixed_volumes();
    test_full_scale_is_louder_than_the_legacy_volumes();
    test_off_is_silent_on_every_path();
    test_scaling_is_monotonic_and_bounded();
    test_step_table_shape();
    test_next_cycles_and_recovers_from_unknown_values();
    test_text_and_capacity_contract();
    puts("test_qpq_volume: PASS");
    return 0;
}
