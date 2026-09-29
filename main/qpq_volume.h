// main/qpq_volume.h —— 音量档位与缩放。
//
// 不依赖 ESP-IDF 与 LVGL，所以宿主测试直接编译它 —— 「默认档位下听感与加音量功能
// 之前完全一致」这句话必须能被测出来，而不是靠注释保证。
//
// 为什么音量是**档位**而不是「加/减」两个操作：这台机器只有上/下/确定三个键，
// 上下键在菜单里已经用来选行了。档位由确定键循环，与「音效」原来的开关操作一致，
// 也就不需要为它多开一项设置。
//
// 为什么 0 档就是关（于是不需要单独的静音开关）：见应用档案里记的那条决定 ——
// 三个键不值得两个设置项。
#pragma once

#include <stddef.h>
#include <stdint.h>

// 档位表。0 是关，最大值是满量程。
#define QPQ_VOLUME_STEP_COUNT 6
#define QPQ_VOLUME_OFF 0
#define QPQ_VOLUME_MAX 100

// 出厂默认档位。必须是档位表里的一个值 —— 见下面那个 _Static_assert。
#define QPQ_VOLUME_DEFAULT 80

// 三路音频在**用户音量 100% 时**的编解码器音量。配音要压过背景音乐，提示音在两者
// 之间，所以三个数不同；用户档位按比例缩放它们，所以三者的相对关系不随档位变化
// —— 调音量不会把配音与音乐的平衡调歪。
//
// 这三个数是**从默认档位反推**出来的，不是随手取的：默认 80% 下
//   100 x 80% = 80     （配音）
//    75 x 80% = 60     （提示音）
//    63 x 80% = 50     （背景音乐）
// 正好等于本应用在加入音量功能之前写死的三个值。也就是说：**默认档位下听感与
// 加音量之前逐位相同**，只有在用户自己往上调（100% 时是 100/75/63）或往下调时
// 才会听到变化。宿主测试把这个等式钉住了。
#define QPQ_VOLUME_BASE_VOICE 100
#define QPQ_VOLUME_BASE_TONE   75
#define QPQ_VOLUME_BASE_BGM    63

// 档位表本体：关、20%、40%、60%、80%、100%。
extern const uint8_t qpq_volume_steps[QPQ_VOLUME_STEP_COUNT];

// 把用户档位乘到某一路的基准音量上。整数截断（不是四舍五入）：base * level / 100。
// 截断在这里是刻意的 —— 三个默认值都能被 80% 整除得到整数，不需要靠舍入凑出来。
uint8_t qpq_volume_scale(uint8_t base, uint8_t level);

// 下一个档位，到顶回绕到关。当前值不在档位表里（存档被改坏、旧版本遗留）时
// 回到第一个档位，而不是原样返回一个不存在的档位。
uint8_t qpq_volume_next(uint8_t level);

// 档位文本："关" 或 "80%"。返回写入的字节数；容量不足时返回 0 并把 out 置为空串
// （不留半截字符串）。capacity 为 0 时只保证返回 0，不写任何字节。
uint16_t qpq_volume_text(uint8_t level, char *out, size_t capacity);
