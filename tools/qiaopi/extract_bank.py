#!/usr/bin/env python3
"""从网页版侨批题库一次性抽取内容，落成设备版的可审阅源文件 bank.txt。

为什么要有这一步，而不是直接让生成器去读 index.html：

- 设备只该依赖一份**稳定、可手改、diff 干净**的内容源。index.html 是网页版的
  产物，它的结构随时可能变，而且它把内容和版式混在一个文件里。
- 抽取只做一次。之后 bank.txt 就是事实来源；改了内容请直接改 bank.txt，
  不要再重跑本脚本（除非确认网页版才是更新的那一份，那时用 --force）。

用法：
    python3 tools/qiaopi/extract_bank.py                 # 只校验并打印报告，不写文件
    python3 tools/qiaopi/extract_bank.py --write         # 写出 bank.txt
    python3 tools/qiaopi/extract_bank.py --write --force # 覆盖已存在的 bank.txt

来源文件路径可用环境变量 QPQ_SOURCE_HTML 覆盖，或作为位置参数传入。
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = REPO_ROOT / "tools" / "qiaopi" / "bank.txt"
DEFAULT_SOURCE = pathlib.Path(
    "D:/novel-to-game-main/game-adaptations/qiaopi/build/app/index.html"
)

BLANK = "____"
OPTION_SEPARATOR = " / "

HEADER = """\
# 侨批 · 填字问答 —— 题库源文件（设备版）
#
# 本文件是内容的事实来源。它由 tools/qiaopi/extract_bank.py 从网页版
# build/app/index.html 一次性抽取而来；之后请直接编辑本文件，不要重跑抽取器，
# 除非你确认网页版才是更新的那一份（那时用 --write --force）。
#
# 格式：每题一个区块，以 "== <id>" 开头，随后是字段，每行一个，顺序固定：
#   category  分类（六级之一），用于答题页顶部的分类徽标
#   sentence  题目句，必须且只能含一个 {blank} 作为填空标记
#   options   四个候选，用 " / " 分隔
#   answer    正确候选的下标（0 起）
#   explain   解析，含典故出处
#   source    侨批来源，寄信人与年份
#   full      完整原文
#
# 约束：
# - 字段值不得跨行；行首空白会算进值里，所以不要缩进。
# - 每题必须恰好四个候选，answer 必须在 0..3。
# - 各字段有设备版式的字数上限，见 tools/qiaopi/content.py —— 超限会被生成器拒绝。
""".format(blank=BLANK)


def load_records(html_path: pathlib.Path) -> list[dict]:
    """从网页产物里解析出题目数组。只认它当前的写法，解析不到就明确失败。"""
    text = html_path.read_text(encoding="utf-8")
    try:
        start = text.index("const QUESTION_BANK = [")
        end = text.index("\n];", start)
    except ValueError as exc:
        raise SystemExit(
            "ERROR: 在 {0} 里找不到 QUESTION_BANK 数组；"
            "网页版结构可能已经变了。".format(html_path)
        ) from exc

    block = text[start:end]
    marks = [m.start() for m in re.finditer(r"\{id:'q", block)]
    if not marks:
        raise SystemExit("ERROR: QUESTION_BANK 里没有解析出任何题目。")

    chunks = [block[a:b] for a, b in zip(marks, marks[1:] + [len(block)])]

    def grab(pattern: str, chunk: str, default: str = "") -> str:
        m = re.search(pattern, chunk, re.S)
        return m.group(1) if m else default

    records = []
    for chunk in chunks:
        raw_options = grab(r"options:\[(.*?)\]", chunk)
        records.append(
            {
                "id": grab(r"id:'(q\d+)'", chunk),
                "category": grab(r"category:'([^']*)'", chunk),
                "sentence": grab(r"sentence:'([^']*)'", chunk),
                "options": re.findall(r"'([^']*)'", raw_options),
                "answer": grab(r"answer:(\d+)", chunk, "-1"),
                "explain": grab(r"explain:'(.*?)',\s*source:'", chunk),
                "source": grab(r"source:'(.*?)',fullSentence:'", chunk),
                "full": grab(r"fullSentence:'(.*?)'\s*\}", chunk),
            }
        )
    return records


def audit(records: list[dict]) -> list[str]:
    """检查数据本身的不变量，返回问题清单（空表示全部通过）。"""
    problems: list[str] = []
    seen_ids: set[str] = set()
    seen_sentences: dict[str, str] = {}

    for rec in records:
        qid = rec["id"] or "<无 id>"

        if not qid.startswith("q"):
            problems.append("{}: 缺少 id".format(qid))
        if qid in seen_ids:
            problems.append("{}: id 重复".format(qid))
        seen_ids.add(qid)

        if len(rec["options"]) != 4:
            problems.append(
                "{}: 候选数 {} != 4".format(qid, len(rec["options"]))
            )

        try:
            answer = int(rec["answer"])
        except ValueError:
            answer = -1
        if not 0 <= answer < 4:
            problems.append("{}: answer={} 越界".format(qid, rec["answer"]))
        elif answer < len(rec["options"]) and rec["options"][answer] not in rec["full"]:
            # 正确候选应当出现在完整原文里，否则说明抽取串了行。
            problems.append(
                "{}: 正确项 {!r} 不在完整原文里".format(qid, rec["options"][answer])
            )

        if rec["sentence"].count(BLANK) != 1:
            problems.append(
                "{}: 句子里的 {} 出现 {} 次，必须恰好 1 次".format(
                    qid, BLANK, rec["sentence"].count(BLANK)
                )
            )

        for field in ("category", "explain", "source", "full"):
            if not rec[field]:
                problems.append("{}: 字段 {} 为空".format(qid, field))

        if len(set(rec["options"])) != len(rec["options"]):
            problems.append("{}: 候选项有重复".format(qid))

        if rec["sentence"] in seen_sentences:
            problems.append(
                "{}: 句子与 {} 完全相同".format(qid, seen_sentences[rec["sentence"]])
            )
        seen_sentences[rec["sentence"]] = qid

    return problems


def render(records: list[dict]) -> str:
    out = [HEADER]
    for rec in records:
        out.append("== {0}".format(rec["id"]))
        out.append("category: {0}".format(rec["category"]))
        out.append("sentence: {0}".format(rec["sentence"]))
        out.append("options: {0}".format(OPTION_SEPARATOR.join(rec["options"])))
        out.append("answer: {0}".format(rec["answer"]))
        out.append("explain: {0}".format(rec["explain"]))
        out.append("source: {0}".format(rec["source"]))
        out.append("full: {0}".format(rec["full"]))
        out.append("")
    return "\n".join(out)


def report(records: list[dict]) -> None:
    import collections

    print("解析出题目：{} 道".format(len(records)))
    print("分类分布：{}".format(
        dict(collections.Counter(r["category"] for r in records))))
    print("正确项下标分布：{}".format(
        dict(sorted(collections.Counter(int(r["answer"]) for r in records).items()))))

    def longest(field, label):
        row = max(records, key=lambda r: len(r[field]))
        avg = sum(len(r[field]) for r in records) / len(records)
        print("  {:<12} 最长 {:>3} 字（平均 {:>5.1f}）来自 {}".format(
            label, len(row[field]), avg, row["id"]))
        return row

    print("字段长度：")
    for field, label in (("sentence", "sentence"), ("explain", "explain"),
                         ("source", "source"), ("full", "full"),
                         ("category", "category")):
        longest(field, label)
    option_lengths = [len(o) for r in records for o in r["options"]]
    print("  {:<12} 最长 {:>3} 字（平均 {:>5.1f}）".format(
        "option", max(option_lengths), sum(option_lengths) / len(option_lengths)))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", nargs="?", default=None,
                        help="网页版 index.html 路径")
    parser.add_argument("--write", action="store_true", help="写出 bank.txt")
    parser.add_argument("--force", action="store_true",
                        help="覆盖已存在的 bank.txt")
    args = parser.parse_args()

    source = pathlib.Path(args.source or os.environ.get("QPQ_SOURCE_HTML")
                          or DEFAULT_SOURCE)
    if not source.is_file():
        print("ERROR: 找不到来源文件 {0}".format(source), file=sys.stderr)
        print("用位置参数或 QPQ_SOURCE_HTML 指定网页版 index.html。", file=sys.stderr)
        return 1

    records = load_records(source)
    report(records)

    problems = audit(records)
    if problems:
        print("\n数据本身有 {} 处问题：".format(len(problems)))
        for item in problems:
            print("  - {}".format(item))
    else:
        print("\n数据不变量：全部通过")

    if not args.write:
        print("\n（未写文件；加 --write 才会生成 {0}）".format(
            DEFAULT_OUTPUT.relative_to(REPO_ROOT)))
        return 1 if problems else 0

    if DEFAULT_OUTPUT.exists() and not args.force:
        print("\nERROR: {0} 已存在。它是内容的事实来源，不要被覆盖。".format(
            DEFAULT_OUTPUT), file=sys.stderr)
        print("确认要用网页版覆盖它，才加 --force。", file=sys.stderr)
        return 1

    DEFAULT_OUTPUT.write_text(render(records), encoding="utf-8")
    print("\n已写出 {0}（{1} 字节）".format(
        DEFAULT_OUTPUT.relative_to(REPO_ROOT), DEFAULT_OUTPUT.stat().st_size))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
