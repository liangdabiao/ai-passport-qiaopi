#!/usr/bin/env python3
"""把网页版的音频素材编码成设备用的单一 IMA-ADPCM blob。

在仓库根目录运行：

    python3 tools/qiaopi/gen_audio.py            # 重新编码并写出全部产物
    python3 tools/qiaopi/gen_audio.py --check    # 只校验，不重新编码

为什么是「一个 blob」而不是 92 个文件：设备上没有文件系统，把音频摊成 92 个
资源既多了一次索引层，又让 Flash 里多出 92 段互不相邻的数据。单个 blob 里
自带索引表，固件用指针直接寻址，读一个片段就是一次内存映射访问。

为什么是 ADPCM 而不是 MP3：见 tools/qiaopi/adpcm.py 顶部。一句话——为了 11 分钟
人声引入 MP3 解码库，换来的是 28 KB 堆、单核上可观的 CPU 开销和一个第三方
预编译依赖；而 4 bit ADPCM 只需要一次查表加法。代价是体积约两倍，本项目放得下。

产出的四件东西：

    assets/audio/qpq_audio.bin        设备要用的 blob（唯一进固件的产物）
    assets/audio/clips.txt            可审阅的清单（含源文件 sha256），--check 用它比对
    main/qpq_audio.h                  片段编号、采样率、blob 符号声明
    tests/qpq_adpcm_fixture.h         跨语言固定向量：C 解码器必须与 Python 编码器逐位一致

源素材目录默认指向网页版构建产物，可用 QPQ_AUDIO_SOURCE 或位置参数覆盖。
"""
from __future__ import annotations

import argparse
import array
import hashlib
import math
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import adpcm  # noqa: E402
import content  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_AUDIO_SOURCE = Path(
    "D:/novel-to-game-main/game-adaptations/qiaopi/build/app/audio")

OUT_BLOB = ROOT / "assets" / "audio" / "qpq_audio.bin"
OUT_TABLE = ROOT / "assets" / "audio" / "clips.txt"
OUT_HEADER = ROOT / "main" / "qpq_audio.h"
OUT_FIXTURE = ROOT / "tests" / "qpq_adpcm_fixture.h"

SAMPLE_RATE = 16000
MAGIC = b"QPQA"
VERSION = 1
BGM_SOURCE_NAME = "aaa.mp3"
BLOB_HEADER = struct.Struct("<4sHHII")     # magic, version, clip_count, rate, reserved
INDEX_ENTRY = struct.Struct("<II")         # offset, samples

TABLE_NOTICE = (
    "# 由 tools/qiaopi/gen_audio.py 生成，请勿手改。\n"
    "# 每行一个片段：index  clip  source  source_sha256  samples  bytes\n"
    "# index 与题目下标一一对应（q01 是 0 号）；BGM 排在最后。\n"
    "# --check 会在源素材目录可用时逐个核对 sha256。\n"
)

HEADER_NOTICE = (
    "// 由 tools/qiaopi/gen_audio.py 生成，请勿手改。\n"
    "// blob 里的索引表是权威来源；这里只声明编号常量与符号，供固件直接引用。\n"
)

FIXTURE_NOTICE = (
    "// 由 tools/qiaopi/gen_audio.py 生成，请勿手改。\n"
    "// 跨语言固定向量：这些字节由 Python 参考实现编码，C 解码器必须解出完全相同的\n"
    "// expected 序列。它证明两个实现逐位一致，而不是「大概一样」。\n"
)


def find_ffmpeg() -> str:
    found = shutil.which("ffmpeg")
    if not found:
        raise SystemExit(
            "ERROR: 找不到 ffmpeg。重编码需要它；只想校验现有产物请加 --check。")
    return found


def decode_source(ffmpeg: str, path: Path) -> array.array:
    """用 ffmpeg 把任意源格式解成 16 kHz 单声道 s16le。"""
    command = [
        ffmpeg, "-v", "error", "-nostdin", "-i", str(path),
        "-f", "s16le", "-acodec", "pcm_s16le", "-ac", "1",
        "-ar", str(SAMPLE_RATE), "-",
    ]
    result = subprocess.run(command, capture_output=True)
    if result.returncode != 0:
        raise SystemExit(
            "ERROR: ffmpeg 解码失败 {0}：\n{1}".format(
                path.name, result.stderr.decode("utf-8", "replace").strip()))
    samples = array.array("h")
    samples.frombytes(result.stdout)
    if sys.byteorder == "big":
        samples.byteswap()
    if not samples:
        raise SystemExit("ERROR: {0} 解码后没有任何样本".format(path.name))
    return samples


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_clip_plan(questions: list[content.Question]) -> list[tuple[str, str]]:
    """返回 [(片段名, 源文件名)]，题目顺序在前，BGM 在最后。"""
    plan = [(q.qid, "{0}.mp3".format(q.qid)) for q in questions]
    plan.append(("bgm", BGM_SOURCE_NAME))
    return plan


def encode_all(ffmpeg: str, source_dir: Path,
               plan: list[tuple[str, str]]) -> tuple[list[tuple], str]:
    """编码全部片段，返回 (逐片段记录, blob 字节)。"""
    records: list[tuple] = []
    payloads: list[bytes] = []

    for index, (clip_name, source_name) in enumerate(plan):
        source_path = source_dir / source_name
        if not source_path.is_file():
            raise SystemExit(
                "ERROR: 缺少源素材 {0}（片段 {1}）".format(source_path, clip_name))
        samples = decode_source(ffmpeg, source_path)
        encoded = adpcm.encode_clip(samples)
        payloads.append(encoded)
        records.append({
            "index": index,
            "clip": clip_name,
            "source": source_name,
            "sha256": sha256_of(source_path),
            "samples": len(samples),
            "bytes": len(encoded),
        })
        print("  [{0:>3}/{1}] {2:<6} {3:<10} {4:>7} 样本 -> {5:>7} 字节 （{6:.1f} 秒）".format(
            index + 1, len(plan), clip_name, source_name,
            len(samples), len(encoded), len(samples) / SAMPLE_RATE))

    clip_count = len(records)
    data_start = BLOB_HEADER.size + INDEX_ENTRY.size * clip_count

    header = BLOB_HEADER.pack(MAGIC, VERSION, clip_count, SAMPLE_RATE, 0)
    index = bytearray()
    offset = data_start
    for record, payload in zip(records, payloads):
        if len(payload) != record["bytes"]:
            raise SystemExit("ERROR: 内部不一致，编码长度与记录不符")
        index += INDEX_ENTRY.pack(offset, record["samples"])
        offset += len(payload)

    blob = header + bytes(index) + b"".join(payloads)
    return records, blob


def render_table(records: list[tuple]) -> str:
    lines = [TABLE_NOTICE]
    for record in records:
        lines.append("{index}\t{clip}\t{source}\t{sha256}\t{samples}\t{bytes}".format(
            **record))
    total_samples = sum(r["samples"] for r in records)
    total_bytes = sum(r["bytes"] for r in records)
    lines.append("")
    lines.append("# 片段数 {0}，总样本 {1}（{2:.1f} 秒），有效载荷 {3} 字节".format(
        len(records), total_samples, total_samples / SAMPLE_RATE, total_bytes))
    lines.append("# blob 总大小 {0} 字节（含 {1} 字节头与索引）".format(
        BLOB_HEADER.size + INDEX_ENTRY.size * len(records) + total_bytes,
        BLOB_HEADER.size + INDEX_ENTRY.size * len(records)))
    return "\n".join(lines) + "\n"


def render_header(records: list[tuple]) -> str:
    narration = [r for r in records if r["clip"] != "bgm"]
    bgm_index = next((r["index"] for r in records if r["clip"] == "bgm"), -1)
    lines = [
        HEADER_NOTICE,
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "/* 片段数 = 题目数 + 1（BGM）。narration 片段下标与题目下标一一对应。 */",
        "#define QPQ_AUDIO_CLIP_COUNT {0}".format(len(records)),
        "#define QPQ_AUDIO_NARRATION_COUNT {0}".format(len(narration)),
        "#define QPQ_AUDIO_BGM_CLIP {0}".format(bgm_index),
        "#define QPQ_AUDIO_SAMPLE_RATE {0}".format(SAMPLE_RATE),
        "",
        "/* blob 本身由 main/CMakeLists.txt 的 target_add_binary_data 编进固件；",
        " * 取字节的入口是 main/qpq_audio_blob.c 里的 qpq_audio_blob() 与",
        " * qpq_audio_blob_size()。链接器的 asm 符号名只出现在那一个文件里 ——",
        " * 宿主测试不该、也无法依赖它。 */",
        "",
    ]
    return "\n".join(lines)


def build_fixture() -> tuple[bytes, list[int], list[int]]:
    """构造一段确定性信号，覆盖小差分与满幅跳变（逼出预测值夹取）。"""
    samples = []
    for index in range(64):
        if index < 48:
            value = int(12000 * math.sin(2 * math.pi * index / 16) * (1 - index / 64))
        else:
            value = 30000 if index % 2 == 0 else -30000
        samples.append(value)
    encoded = adpcm.encode_clip(samples)
    return encoded, samples, adpcm.decode_clip(encoded, len(samples))


def render_fixture() -> str:
    encoded, source, expected = build_fixture()

    def int16_array(name: str, values) -> list[str]:
        rows = [f"static const int16_t {name}[{len(values)}] = {{"]
        for start in range(0, len(values), 8):
            chunk = ", ".join(str(v) for v in values[start:start + 8])
            rows.append(f"    {chunk},")
        rows.append("};")
        return rows

    def byte_array(name: str, values: bytes) -> list[str]:
        rows = [f"static const uint8_t {name}[{len(values)}] = {{"]
        for start in range(0, len(values), 12):
            chunk = ", ".join(f"0x{b:02X}" for b in values[start:start + 12])
            rows.append(f"    {chunk},")
        rows.append("};")
        return rows

    lines = [
        FIXTURE_NOTICE,
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "#define QPQ_ADPCM_FIXTURE_SAMPLES {0}".format(len(source)),
        "",
        "/* 原始信号：前 48 个样本是衰减正弦，后 16 个是正负满幅跳变。 */",
        *int16_array("qpq_adpcm_fixture_source", source),
        "",
        "/* 参考实现编出来的字节：4 字节头 + 半字节流。 */",
        *byte_array("qpq_adpcm_fixture_encoded", encoded),
        "",
        "/* 参考实现解出来的序列：C 解码器必须逐位相同。 */",
        *int16_array("qpq_adpcm_fixture_expected", expected),
        "",
    ]
    return "\n".join(lines)


def write_or_check(path: Path, text: str, check: bool) -> bool:
    existing = path.read_text(encoding="utf-8") if path.is_file() else None
    if existing == text:
        print("unchanged: {0}".format(path.relative_to(ROOT)))
        return True
    if check:
        print("STALE: {0}".format(path.relative_to(ROOT)), file=sys.stderr)
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    print("wrote: {0}".format(path.relative_to(ROOT)))
    return True


def write_blob_or_check(data: bytes, check: bool) -> bool:
    existing = OUT_BLOB.read_bytes() if OUT_BLOB.is_file() else None
    if existing == data:
        print("unchanged: {0}".format(OUT_BLOB.relative_to(ROOT)))
        return True
    if check:
        print("STALE: {0}".format(OUT_BLOB.relative_to(ROOT)), file=sys.stderr)
        return False
    OUT_BLOB.parent.mkdir(parents=True, exist_ok=True)
    OUT_BLOB.write_bytes(data)
    print("wrote: {0}（{1} 字节）".format(
        OUT_BLOB.relative_to(ROOT), len(data)))
    return True


def parse_table() -> list[dict]:
    """把 clips.txt 读回成记录，供 --check 比对。"""
    if not OUT_TABLE.is_file():
        raise SystemExit("ERROR: 找不到 {0}".format(OUT_TABLE))
    records = []
    for line in OUT_TABLE.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) != 6:
            raise SystemExit("ERROR: clips.txt 行格式不对：{0!r}".format(line[:60]))
        records.append({
            "index": int(parts[0]),
            "clip": parts[1],
            "source": parts[2],
            "sha256": parts[3],
            "samples": int(parts[4]),
            "bytes": int(parts[5]),
        })
    return records


def check_only(source_dir: Path, plan: list[tuple[str, str]]) -> int:
    """不重编码，校验现有 blob 与清单是否一致、源素材是否被动过。"""
    if not OUT_BLOB.is_file():
        print("STALE: 缺少 {0}".format(OUT_BLOB.relative_to(ROOT)), file=sys.stderr)
        return 1

    blob = OUT_BLOB.read_bytes()
    if len(blob) < BLOB_HEADER.size:
        print("STALE: blob 比头部还短", file=sys.stderr)
        return 1
    magic, version, clip_count, rate, _ = BLOB_HEADER.unpack_from(blob, 0)
    problems: list[str] = []
    if magic != MAGIC:
        problems.append("magic 是 {0!r}，应为 {1!r}".format(magic, MAGIC))
    if version != VERSION:
        problems.append("version 是 {0}，应为 {1}".format(version, VERSION))
    if rate != SAMPLE_RATE:
        problems.append("采样率是 {0}，应为 {1}".format(rate, SAMPLE_RATE))

    records = parse_table()
    if clip_count != len(records):
        problems.append("blob 有 {0} 个片段，clips.txt 有 {1} 个".format(
            clip_count, len(records)))
    if clip_count != len(plan):
        problems.append("blob 有 {0} 个片段，但应当有 {1} 个".format(
            clip_count, len(plan)))

    expected_offset = BLOB_HEADER.size + INDEX_ENTRY.size * clip_count
    for position in range(min(clip_count, len(records))):
        offset, samples = INDEX_ENTRY.unpack_from(
            blob, BLOB_HEADER.size + INDEX_ENTRY.size * position)
        record = records[position]
        if samples != record["samples"]:
            problems.append("片段 {0} 样本数 blob={1} 清单={2}".format(
                position, samples, record["samples"]))
        if offset != expected_offset:
            problems.append("片段 {0} 偏移 blob={1} 应为 {2}".format(
                position, offset, expected_offset))
        if adpcm.encoded_size(samples) != record["bytes"]:
            problems.append("片段 {0} 字节数与样本数不相容".format(position))
        expected_offset += record["bytes"]
        if position < len(plan) and record["clip"] != plan[position][0]:
            problems.append("片段 {0} 名字是 {1}，应为 {2}".format(
                position, record["clip"], plan[position][0]))

    if expected_offset != len(blob):
        problems.append("索引推出来的结尾是 {0}，blob 实际 {1} 字节".format(
            expected_offset, len(blob)))

    # 源素材核对。CI 上没有这些源文件，此时明确跳过，不假装通过。
    if source_dir.is_dir():
        checked = 0
        for record in records:
            path = source_dir / record["source"]
            if not path.is_file():
                problems.append("缺少源素材 {0}".format(record["source"]))
                continue
            if sha256_of(path) != record["sha256"]:
                problems.append("源素材 {0} 已改动，需要重编码".format(record["source"]))
            checked += 1
        print("源素材核对：{0}/{1} 个 sha256 一致".format(checked, len(records)))
    else:
        print("skip: 源素材目录 {0} 不存在，未核对 sha256"
              "（blob 与 clips.txt 的结构一致性仍然检查过了）".format(source_dir))

    if problems:
        print("音频产物有 {0} 处问题：".format(len(problems)), file=sys.stderr)
        for item in problems:
            print("  - {0}".format(item), file=sys.stderr)
        return 1
    print("音频产物：一致（{0} 个片段，{1} 字节）".format(clip_count, len(blob)))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source_dir", nargs="?", default=None,
                        help="源素材目录")
    parser.add_argument("--check", action="store_true", help="只校验，不重编码")
    args = parser.parse_args()

    source_dir = Path(args.source_dir or os.environ.get("QPQ_AUDIO_SOURCE")
                      or DEFAULT_AUDIO_SOURCE)

    try:
        questions = content.load_bank()
    except content.ContentError as error:
        print("题库源文件错误：{0}".format(error), file=sys.stderr)
        return 1
    plan = build_clip_plan(questions)

    if args.check:
        return check_only(source_dir, plan)

    if not source_dir.is_dir():
        print("ERROR: 源素材目录不存在：{0}".format(source_dir), file=sys.stderr)
        return 1

    ffmpeg = find_ffmpeg()
    print("源素材：{0}".format(source_dir))
    print("目标：{0} Hz 单声道 IMA-ADPCM，{1} 个片段".format(SAMPLE_RATE, len(plan)))
    records, blob = encode_all(ffmpeg, source_dir, plan)

    ok = write_blob_or_check(blob, False)
    ok = write_or_check(OUT_TABLE, render_table(records), False) and ok
    ok = write_or_check(OUT_HEADER, render_header(records), False) and ok
    ok = write_or_check(OUT_FIXTURE, render_fixture(), False) and ok

    total_samples = sum(r["samples"] for r in records)
    print("合计：{0} 个片段，{1:.1f} 秒，blob {2} 字节（{3:.2f} MB）".format(
        len(records), total_samples / SAMPLE_RATE, len(blob), len(blob) / 1048576))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
