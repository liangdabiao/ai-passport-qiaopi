#!/usr/bin/env python3
"""从题库源文件生成 ``main/qpq_text.c`` 与 ``main/qpq_text.h``。

C 表只是 ``tools/qiaopi/bank.txt`` 的投影，除此之外没有任何来源：没有一个字符串
是在 C 里直接改的。在仓库根目录运行：

    python3 tools/qiaopi/gen_content.py            # 重新生成两张表
    python3 tools/qiaopi/gen_content.py --check    # 只校验，过期就失败

``--check`` 是 CI 与 ``tools/validate.sh`` 用的那一个：改了题库却忘了重新生成，
会在这里失败，而不是把一个过期的表发到设备上。这也是本项目对「记得重新生成」
这类人为约定的一贯处理方式——把它变成一条机器检查。
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import content  # noqa: E402  （上面的 sys.path 已经指到同目录）

ROOT = Path(__file__).resolve().parents[2]
OUT_C = ROOT / "main" / "qpq_text.c"
OUT_H = ROOT / "main" / "qpq_text.h"

HEADER_NOTICE = (
    "// 由 tools/qiaopi/gen_content.py 从 tools/qiaopi/bank.txt 生成。\n"
    "// 请勿手改；改题库源文件后重跑生成器（validate.sh 会用 --check 拦住过期的表）。\n"
)


def c_string(text: str) -> str:
    """把任意 UTF-8 文本变成 C 字符串字面量。"""
    escaped = text.replace("\\", "\\\\").replace('"', '\\"').replace("\t", "\\t")
    return f'"{escaped}"'


def render_header(questions: list[content.Question]) -> str:
    lines = [
        HEADER_NOTICE,
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "/* 题库规模。每局抽 QPQ_RUN_LENGTH 题；题库不足时由逻辑层夹取。 */",
        f"#define QPQ_QUESTION_COUNT {len(questions)}",
        f"#define QPQ_OPTION_COUNT {content.OPTION_COUNT}",
        f"#define QPQ_ANSWER_SLOT_COUNT {len(content.ANSWER_SLOTS)}",
        f"#define QPQ_CATEGORY_COUNT {len(content.categories(questions))}",
        f"#define QPQ_RUN_LENGTH {content.RUN_LENGTH}",
        f"#define QPQ_RANK_COUNT {len(content.RANKS)}",
        "",
        "/* 填空标记：题目句里用它切出设备上的空格位。 */",
        f'#define QPQ_BLANK {c_string(content.BLANK)}',
        "",
        "/* 计分规则，与网页版一致：答对得 QPQ_SCORE_CORRECT，连对达",
        " * QPQ_STREAK_BONUS_AT 次及以上追加 QPQ_SCORE_STREAK_BONUS，答错扣",
        " * QPQ_SCORE_WRONG_PENALTY，且分数不会低于零。 */",
        f"#define QPQ_SCORE_CORRECT {content.SCORE_CORRECT}",
        f"#define QPQ_SCORE_STREAK_BONUS {content.SCORE_STREAK_BONUS}",
        f"#define QPQ_STREAK_BONUS_AT {content.STREAK_BONUS_AT}",
        f"#define QPQ_SCORE_WRONG_PENALTY {content.SCORE_WRONG_PENALTY}",
        "",
        "/* 设备版式预算，来源是 tools/qiaopi/content.py 顶部那张算术推导。",
        " * 宿主测试拿真实题库断言折行结果不超这些值。 */",
        f"#define QPQ_CATEGORY_MAX_CHARS {content.CATEGORY_MAX_CHARS}",
        f"#define QPQ_SENTENCE_MAX_CHARS {content.SENTENCE_MAX_CHARS}",
        f"#define QPQ_SENTENCE_FILLED_MAX_CHARS {content.SENTENCE_FILLED_MAX_CHARS}",
        f"#define QPQ_OPTION_MAX_CHARS {content.OPTION_MAX_CHARS}",
        f"#define QPQ_EXPLAIN_MAX_CHARS {content.EXPLAIN_MAX_CHARS}",
        f"#define QPQ_FULL_MAX_CHARS {content.FULL_MAX_CHARS}",
        f"#define QPQ_SOURCE_MAX_CHARS {content.SOURCE_MAX_CHARS}",
        "",
        "typedef struct {",
        "    const char *category;                    /* 分类，答题页顶栏 */",
        "    const char *sentence;                    /* 题目句，含 QPQ_BLANK */",
        "    const char *options[QPQ_OPTION_COUNT];   /* 四个候选，顺序固定 */",
        "    const char *explain;                     /* 解析，含典故出处 */",
        "    const char *source;                      /* 侨批来源，寄信人与年份 */",
        "    const char *full;                        /* 完整原文 */",
        "    uint8_t answer;                          /* 正确候选下标 0..3 */",
        "} qpq_question_t;",
        "",
        "/* 结算评级：percent 是正确率下限，从高到低排列，取第一个满足的档。 */",
        "typedef struct {",
        "    uint8_t percent;",
        "    const char *rank;",
        "    const char *subtitle;",
        "    const char *description;",
        "} qpq_rank_t;",
        "",
        "extern const qpq_question_t qpq_questions[QPQ_QUESTION_COUNT];",
        "extern const char *const qpq_answer_slots[QPQ_ANSWER_SLOT_COUNT];",
        "extern const char *const qpq_categories[QPQ_CATEGORY_COUNT];",
        "extern const qpq_rank_t qpq_ranks[QPQ_RANK_COUNT];",
        "",
    ]
    return "\n".join(lines)


def render_source(questions: list[content.Question]) -> str:
    question_rows = []
    for q in questions:
        option_rows = "\n".join(
            f"            {c_string(option)}," for option in q.options)
        question_rows.append(
            "    {\n"
            f"        .category = {c_string(q.category)},\n"
            f"        .sentence = {c_string(q.sentence)},\n"
            "        .options = {\n"
            f"{option_rows}\n"
            "        },\n"
            f"        .explain = {c_string(q.explain)},\n"
            f"        .source = {c_string(q.source)},\n"
            f"        .full = {c_string(q.full)},\n"
            f"        .answer = {q.answer},\n"
            "    },"
        )

    lines = [
        HEADER_NOTICE,
        '#include "qpq_text.h"',
        "",
        "const qpq_question_t qpq_questions[QPQ_QUESTION_COUNT] = {",
        *question_rows,
        "};",
        "",
        "/* 选项标签。四个候选按甲、乙、丙、丁标出，与网页版一致。 */",
        "const char *const qpq_answer_slots[QPQ_ANSWER_SLOT_COUNT] = {",
        *(f"    {c_string(slot)}," for slot in content.ANSWER_SLOTS),
        "};",
        "",
        "/* 分类表：用于显示与统计，随机抽题不按分类分层。 */",
        "const char *const qpq_categories[QPQ_CATEGORY_COUNT] = {",
        *(f"    {c_string(name)}," for name in content.categories(questions)),
        "};",
        "",
        "/* 结算评级，按正确率下限从高到低。 */",
        "const qpq_rank_t qpq_ranks[QPQ_RANK_COUNT] = {",
        *(
            "    {\n"
            f"        .percent = {threshold},\n"
            f"        .rank = {c_string(rank)},\n"
            f"        .subtitle = {c_string(subtitle)},\n"
            f"        .description = {c_string(description)},\n"
            "    },"
            for threshold, rank, subtitle, description in content.RANKS
        ),
        "};",
        "",
    ]
    return "\n".join(lines)


def write_or_check(path: Path, text: str, check: bool) -> bool:
    """写入文件，或只报告它是否已经是最新的。"""
    existing = path.read_text(encoding="utf-8") if path.is_file() else None
    if existing == text:
        print(f"unchanged: {path.relative_to(ROOT)}")
        return True
    if check:
        print(f"STALE: {path.relative_to(ROOT)}", file=sys.stderr)
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote: {path.relative_to(ROOT)}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="只校验，不写文件")
    args = parser.parse_args()

    try:
        questions = content.load_bank()
    except content.ContentError as error:
        print(f"题库源文件错误：{error}", file=sys.stderr)
        return 1

    problems = content.validate(questions)
    if problems:
        print(f"题库有 {len(problems)} 处问题：", file=sys.stderr)
        for item in problems:
            print(f"  - {item}", file=sys.stderr)
        return 1

    print(
        f"题目：{len(questions)}  分类：{len(content.categories(questions))}  "
        f"每局：{content.RUN_LENGTH}  评级档位：{len(content.RANKS)}"
    )

    ok = write_or_check(OUT_H, render_header(questions), args.check)
    ok = write_or_check(OUT_C, render_source(questions), args.check) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
