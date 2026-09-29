"""侨批 · 填字问答 —— 内容的解析、校验与设备版式预算。

这是内容层的唯一入口：``gen_content.py``（投影成 C 表）与 ``gen_font.py``
（推导字符清单）都 import 本模块，所以「题库里有什么」和「该给字库准备哪些字」
不会各说各话。

源文件是 ``tools/qiaopi/bank.txt``：每题一个区块，以 ``== qNN`` 开头，随后是
固定顺序的 ``key: value`` 行。格式刻意做得无聊，好让每个错误都是响亮的：
未知字段名、缺字段、候选数不是四、超长文案，一律让生成器失败，而不是安静地
把一个有缺陷的表发到设备上。

     == q01
     category: 思亲念家
     sentence: 自别慈颜，时怀____。
     options: 牵挂 / 思念 / 孺慕 / 惦记
     answer: 2
     explain: ...
     source: ...
     full: ...

字数上限的由来（240x320 屏，正文可用宽 210px）。中文字形在同一字号下 advance
完全相同，所以「一行放得下几个字」是算术，不是审美：

    字号   字宽   每行字数   行高
    24px   24      8         29
    16px   16      13        20

答题页正文高 248px，分配：

    句子   24px，每行 8 字，预算 3 行 = 87px
    选项   4 行 x 34px + 3 个 6px 间隔 = 154px
    页内间隔                            =  6px
    合计 247px，余 1px

判卷页内容必然超过一屏（解析 + 完整原文 + 出处），所以它是唯一可滚动的页面，
它的上限按「行数」而不是「一屏」来定。

句子的上限为什么是 21：**单行最多 8 字是硬保证（算术），行数不是**——为了让下一行
不以收尾标点开头，断点会向前收，收多少取决于标点落在哪。所以 21 只是内容侧的
守门值（3 行 x 7 字），真正的行数保证由 tests/test_qpq_wrap.c 对**整个题库**逐条
断言（当前实测最多 3 行）。按 24 开会让长句溢出答题页留给句子的那 87px。

因此下列上限**不是美学偏好，是算术结果**：超过就一定有内容被挤出屏幕。这里拒绝
超限内容，宿主测试再拿真实题库的折行结果断言一次。
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BANK_PATH = ROOT / "tools" / "qiaopi" / "bank.txt"

BLANK = "____"
OPTION_SEPARATOR = " / "
OPTION_COUNT = 4
ANSWER_SLOTS = ("甲", "乙", "丙", "丁")

# 设备版式上限，见模块开头的算术推导。
CATEGORY_MAX_CHARS = 6    # 16px 顶栏，与进度数字同一行
SENTENCE_MAX_CHARS = 22   # 24px，答题页显示的**槽位形态**（前缀+4字槽位+后缀）
SENTENCE_FILLED_MAX_CHARS = 21  # 24px，判卷页显示的**已填空形态**（前缀+答案+后缀）
OPTION_MAX_CHARS = 6      # 24px 选项行，含「甲」标签后仍能一行放完
EXPLAIN_MAX_CHARS = 60    # 24px，判卷页可滚动，最多 8 行
FULL_MAX_CHARS = 48       # 24px，最多 6 行
SOURCE_MAX_CHARS = 26     # 16px，最多 2 行

# 每局抽题数。网页版固定 20 题；题库不足 20 题时由逻辑层夹取。
RUN_LENGTH = 20

# 计分规则，与网页版一致。
SCORE_CORRECT = 10
SCORE_STREAK_BONUS = 2       # 连对达到 STREAK_BONUS_AT 次及以上时追加
STREAK_BONUS_AT = 2
SCORE_WRONG_PENALTY = 3

# 结算评级档位：(正确率下限, 评级, 副题, 评语)
RANKS = (
    (90, "侨批大师", "侨批文化的守护者",
     "你对侨批文化有深厚的了解，堪称侨批文化的守护者。每一封侨批背后的历史与情感，你都能深刻领会。"),
    (75, "识字先贤", "博学多识的华侨子弟",
     "你有扎实的文化功底，对侨批的理解超越常人。继续研读，你将成为侨批大师。"),
    (60, "番客子弟", "知书达理的华侨后裔",
     "你对侨批有基本的了解，但还有很多侨批中的典故和用法等待你去发现。"),
    (40, "初识侨批", "刚刚踏入侨批的世界",
     "你开始接触侨批文化了。每一道错题都是一次学习的机会，再来一次吧！"),
    (0, "需勤学", "还要多加努力",
     "侨批是中华文化的瑰宝，值得认真研读。不要气馁，再来一次！"),
)

FIELDS = ("category", "sentence", "options", "answer", "explain", "source", "full")


class ContentError(Exception):
    """题库源文件不合法。生成器应当据此失败，而不是发出一个有缺陷的表。"""


class Question:
    """一道题，字段名与 bank.txt 的键一一对应。"""

    __slots__ = FIELDS + ("qid",)

    def __init__(self, qid: str, category: str, sentence: str,
                 options: list[str], answer: int, explain: str,
                 source: str, full: str) -> None:
        self.qid = qid
        self.category = category
        self.sentence = sentence
        self.options = options
        self.answer = answer
        self.explain = explain
        self.source = source
        self.full = full

    @property
    def correct_option(self) -> str:
        return self.options[self.answer]

    def sentence_parts(self) -> tuple[str, str]:
        """把句子拆成填空位之前与之后两段。"""
        head, _, tail = self.sentence.partition(BLANK)
        return head, tail

    def filled_sentence(self) -> str:
        """把填空位换成正确答案后的句子——版式要按这个长度算，不是按源码长度。"""
        return self.sentence.replace(BLANK, self.correct_option)

    def __repr__(self) -> str:  # pragma: no cover - 只为人读
        return f"Question({self.qid}, {self.sentence!r})"


def load_bank(path: Path | None = None) -> list[Question]:
    """读取并严格解析题库源文件。任何格式问题都直接报错，不猜。"""
    bank_path = Path(path) if path else DEFAULT_BANK_PATH
    if not bank_path.is_file():
        raise ContentError(f"找不到题库源文件：{bank_path}")

    questions: list[Question] = []
    current: dict[str, str] | None = None
    current_id = ""

    def flush() -> None:
        nonlocal current, current_id
        if current is None:
            return
        missing = [name for name in FIELDS if name not in current]
        if missing:
            raise ContentError(
                f"{current_id or '<无 id>'}: 缺少字段 {', '.join(missing)}")
        try:
            answer = int(current["answer"])
        except ValueError:
            raise ContentError(
                f"{current_id}: answer 不是数字：{current['answer']!r}")
        questions.append(Question(
            qid=current_id,
            category=current["category"],
            sentence=current["sentence"],
            options=[piece.strip() for piece in current["options"].split(OPTION_SEPARATOR)],
            answer=answer,
            explain=current["explain"],
            source=current["source"],
            full=current["full"],
        ))
        current = None
        current_id = ""

    for lineno, raw in enumerate(
            bank_path.read_text(encoding="utf-8").splitlines(), start=1):
        if raw.startswith("#"):
            continue
        if not raw.strip():
            flush()
            continue

        if raw.startswith("== "):
            flush()
            current_id = raw[3:].strip()
            current = {}
            continue

        if current is None:
            raise ContentError(
                f"{bank_path.name}:{lineno}: 字段出现在任何 \"== <id>\" 区块之外：{raw[:40]!r}")
        key, sep, value = raw.partition(":")
        if not sep:
            raise ContentError(
                f"{bank_path.name}:{lineno}: 不是 key: value 形式：{raw[:40]!r}")
        key = key.strip()
        if key not in FIELDS:
            raise ContentError(f"{bank_path.name}:{lineno}: 未知字段名 {key!r}")
        if key in current:
            raise ContentError(f"{bank_path.name}:{lineno}: 字段 {key!r} 在同一题里出现两次")
        current[key] = value[1:] if value.startswith(" ") else value

    flush()

    if not questions:
        raise ContentError(f"题库里一道题都没有：{bank_path}")
    return questions


def validate(questions: list[Question]) -> list[str]:
    """返回问题清单（空表示通过）。既查结构，也查设备版式上限。"""
    problems: list[str] = []
    seen_ids: set[str] = set()
    seen_sentences: dict[str, str] = {}

    for q in questions:
        qid = q.qid

        if not re.fullmatch(r"q\d{2,}", qid):
            problems.append(f"{qid}: id 应当形如 q01")
        if qid in seen_ids:
            problems.append(f"{qid}: id 重复")
        seen_ids.add(qid)

        if len(q.options) != OPTION_COUNT:
            problems.append(f"{qid}: 候选数 {len(q.options)}，必须正好 {OPTION_COUNT}")
        if not 0 <= q.answer < len(q.options):
            problems.append(f"{qid}: answer={q.answer} 越界")
        elif q.correct_option not in q.full:
            # 正确项一定出现在完整原文里。不成立就说明抽取串了行。
            problems.append(f"{qid}: 正确项 {q.correct_option!r} 不在完整原文里")

        if q.sentence.count(BLANK) != 1:
            problems.append(
                f"{qid}: 句子里 {BLANK} 出现 {q.sentence.count(BLANK)} 次，必须恰好 1 次")
        if len(set(q.options)) != len(q.options):
            problems.append(f"{qid}: 候选项有重复")

        for name in ("category", "explain", "source", "full"):
            if not getattr(q, name).strip():
                problems.append(f"{qid}: 字段 {name} 为空")

        if q.sentence in seen_sentences:
            problems.append(f"{qid}: 句子与 {seen_sentences[q.sentence]} 完全相同")
        seen_sentences[q.sentence] = qid

        # 设备版式上限。答题页与判卷页显示的句子是两种长度不同的形态，两种都要卡：
        #   - 槽位形态：前缀 + 4 字空白槽位 + 后缀，就是源文件里的原句（选「答案」
        #     时还没选，所以只能显示槽位）
        #   - 已填形态：前缀 + 正确答案 + 后缀
        # 只卡一种就会出现「答题页放得下、判卷页溢出」这种只在半路上暴露的问题。
        for label, text, limit in (
            ("category", q.category, CATEGORY_MAX_CHARS),
            ("sentence slot", q.sentence, SENTENCE_MAX_CHARS),
            ("sentence filled", q.filled_sentence(), SENTENCE_FILLED_MAX_CHARS),
            ("explain", q.explain, EXPLAIN_MAX_CHARS),
            ("source", q.source, SOURCE_MAX_CHARS),
            ("full", q.full, FULL_MAX_CHARS),
        ):
            if len(text) > limit:
                problems.append(
                    f"{qid}: {label} 长 {len(text)} 字，超出设备版式上限 {limit} 字")

        for index, option in enumerate(q.options):
            if len(option) > OPTION_MAX_CHARS:
                problems.append(
                    f"{qid}: 候选 {index} 长 {len(option)} 字，超出上限 {OPTION_MAX_CHARS} 字")

    return problems


def categories(questions: list[Question]) -> list[str]:
    """按首次出现顺序返回分类名，供生成器建立分类表。"""
    ordered: list[str] = []
    for q in questions:
        if q.category not in ordered:
            ordered.append(q.category)
    return ordered


def content_characters(questions: list[Question]) -> set[str]:
    """内容里出现过的所有字符，用于字库清单。

    字库清单 = 本函数的结果 + 界面字符串（后者由 gen_font.py 扫 ``main/*.c|*.h``
    的字符串字面量得到）。填空标记里的下划线不算——设备上空格位是画出来的。
    """
    chars: set[str] = set()
    for q in questions:
        chars.update(q.category)
        chars.update(q.sentence.replace(BLANK, ""))
        for option in q.options:
            chars.update(option)
        chars.update(q.explain)
        chars.update(q.source)
        chars.update(q.full)
    # 结算页的评级文案属于内容，不是界面常量。
    for _, rank, subtitle, description in RANKS:
        chars.update(rank)
        chars.update(subtitle)
        chars.update(description)
    chars.update(ANSWER_SLOTS)
    return {c for c in chars if not c.isspace()}


def rank_for_percent(percent: int) -> tuple[str, str, str]:
    """按正确率取评级，返回 (评级, 副题, 评语)。与网页版档位一致。"""
    for threshold, rank, subtitle, description in RANKS:
        if percent >= threshold:
            return rank, subtitle, description
    return RANKS[-1][1], RANKS[-1][2], RANKS[-1][3]
