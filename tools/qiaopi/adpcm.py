"""IMA-ADPCM（IMA/DVI4，4 bit）编解码参考实现。

设备端没有现成的解码器，而这里刻意**不**引入 MP3 解码库：MP3 解码要额外的
预编译二进制、约 28 KB 堆，以及单核上可观且会与 LVGL 抢 CPU 的开销。本项目的
音频是 11 分钟人声，用 4 bit ADPCM 只需约 5 MB 且解码是一次查表加法——
CPU 开销可以忽略，没有第三方依赖，也没有解码器缺陷风险。

本模块是**参考实现**：``main/qpq_adpcm.c`` 里的 C 解码器必须与它逐位一致。
两者的等价性由 ``tests/test_qpq_adpcm.c`` 用 ``gen_audio.py`` 生成的固定向量
断言——也就是说，「Python 编出来的字节，C 解出来必须一模一样」是被测过的，
不是被假设的。

片段字节布局（自描述，不需要容器）：::

    int16   first_sample     片段首样本，直接作为初始预测值
    uint8   step_index       初始步长索引，由编码器按开头一小段估出来（见
                             ``_estimate_initial_step_index``），不是固定 0 ——
                             从 0 起会有可听见的冷启动瞬态
    uint8   reserved         对齐用，恒为 0
    uint8   nibbles[]        其余样本，每字节两个：先高位后低位；样本数为奇数时
                             末字节低位是填充

所以一个 N 样本的片段占 ``4 + ceil((N-1)/2)`` 字节。
"""
from __future__ import annotations

# IMA 步长表，共 89 项。步长索引被夹在 0..88。
STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
)

# 步长索引调整表，用 nibble 的低三位索引。
INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8)

STEP_INDEX_MAX = len(STEP_TABLE) - 1          # 88
PREDICTOR_MIN = -32768
PREDICTOR_MAX = 32767
HEADER_BYTES = 4


def encoded_size(sample_count: int) -> int:
    """N 个样本编码后占多少字节：4 字节头 + ceil((N-1)/2) 字节半字节流。

    ceil((N-1)/2) 与 N//2 在 N>=1 时恒等（N=1 时都是 0），这里写成含有
    "N-1" 的形式是为了让「首样本不算 nibble」这件事在代码里看得见。
    """
    if sample_count <= 0:
        return 0
    return HEADER_BYTES + (sample_count - 1 + 1) // 2


def _clamp_predictor(value: int) -> int:
    return max(PREDICTOR_MIN, min(PREDICTOR_MAX, value))


def _clamp_step_index(value: int) -> int:
    return max(0, min(STEP_INDEX_MAX, value))


def encode_sample(sample: int, predictor: int, step_index: int) -> tuple[int, int, int]:
    """编码一个样本，返回 (nibble, 新预测值, 新步长索引)。

    关键点：这里重建预测值时**逐项加上与解码器完全相同的 vpdiff**，而不是
    直接把 predictor 设成 sample。编码器必须模拟解码器，否则编码器心里的
    预测值与解码器实际得到的值会分岔，后面的差分全部算错。
    """
    step = STEP_TABLE[step_index]
    diff = sample - predictor
    nibble = 8 if diff < 0 else 0
    if diff < 0:
        diff = -diff

    delta = 0
    if diff >= step:
        delta |= 4
        diff -= step
    if diff >= step >> 1:
        delta |= 2
        diff -= step >> 1
    if diff >= step >> 2:
        delta |= 1
        diff -= step >> 2

    reconstructed = step >> 3
    if delta & 4:
        reconstructed += step
    if delta & 2:
        reconstructed += step >> 1
    if delta & 1:
        reconstructed += step >> 2

    if nibble & 8:
        predictor -= reconstructed
    else:
        predictor += reconstructed
    predictor = _clamp_predictor(predictor)
    step_index = _clamp_step_index(step_index + INDEX_TABLE[delta])
    return nibble | delta, predictor, step_index


def decode_nibble(nibble: int, predictor: int, step_index: int) -> tuple[int, int]:
    """解码一个 nibble，返回 (新预测值, 新步长索引)。"""
    step = STEP_TABLE[step_index]
    reconstructed = step >> 3
    if nibble & 4:
        reconstructed += step
    if nibble & 2:
        reconstructed += step >> 1
    if nibble & 1:
        reconstructed += step >> 2

    if nibble & 8:
        predictor -= reconstructed
    else:
        predictor += reconstructed
    predictor = _clamp_predictor(predictor)
    step_index = _clamp_step_index(step_index + INDEX_TABLE[nibble & 7])
    return predictor, step_index


def _estimate_initial_step_index(samples, window: int = 32) -> int:
    """从开头一小段估计初始步长索引，避免冷启动。

    为什么必须估：``step_index`` 从 0 起时步长只有 7，而每个样本最多只能移动
    1.875 倍步长。于是前七八个样本根本追不上信号——实测一段 1000 Hz 正弦，
    冷启动的峰值误差高达 11760/12000，而稳态误差只有 464。对真实人声，这表现
    为开头几百微秒的一记杂音。

    估法是取开头窗口内最大的相邻差分，找让「每样本可表示的最大变化」刚好够住
    它的那个步长。宁可略微偏大：偏大只是量化噪声略高，偏小就是追不上。
    """
    span = min(len(samples), window)
    if span < 2:
        return 0
    peak = max(abs(int(samples[i]) - int(samples[i - 1]))
               for i in range(1, span))
    if peak == 0:
        return 0
    # 每样本可表示的最大变化是 15/8 倍步长，故目标步长是 8/15 倍峰值差分。
    target = peak * 8 // 15
    for index, step in enumerate(STEP_TABLE):
        if step >= target:
            return index
    return STEP_INDEX_MAX


def encode_clip(samples) -> bytes:
    """把一个片段（int16 序列）编码成自描述字节块。"""
    if not samples:
        raise ValueError("空片段无法编码")
    out = bytearray(HEADER_BYTES)

    predictor = int(samples[0])
    step_index = _estimate_initial_step_index(samples)
    out[0:2] = int(predictor).to_bytes(2, "little", signed=True)
    out[2] = step_index
    out[3] = 0

    pending: int | None = None
    for index in range(1, len(samples)):
        nibble, predictor, step_index = encode_sample(
            int(samples[index]), predictor, step_index)
        if pending is None:
            pending = nibble << 4
        else:
            out.append(pending | nibble)
            pending = None
    if pending is not None:
        out.append(pending)
    return bytes(out)


def decode_clip(data: bytes, sample_count: int) -> list[int]:
    """解码本模块 ``encode_clip`` 产出的字节块。供生成固定向量与自查用。"""
    if sample_count <= 0:
        return []
    expected_bytes = encoded_size(sample_count)
    if len(data) < expected_bytes:
        raise ValueError(
            f"片段长度 {len(data)} 字节，{sample_count} 个样本需要 {expected_bytes} 字节")

    predictor = int.from_bytes(data[0:2], "little", signed=True)
    step_index = data[2]
    out = [predictor]

    consumed = 0
    for offset in range(HEADER_BYTES, len(data)):
        if consumed >= sample_count - 1:
            break
        packed = data[offset]
        predictor, step_index = decode_nibble(packed >> 4, predictor, step_index)
        out.append(predictor)
        consumed += 1
        if consumed >= sample_count - 1:
            break
        predictor, step_index = decode_nibble(packed & 0x0F, predictor, step_index)
        out.append(predictor)
        consumed += 1
    return out
