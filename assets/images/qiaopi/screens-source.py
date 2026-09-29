#!/usr/bin/env python3
"""按 qiaopi-quiz 源码里的版式常量，如实重画四个页面（240x320）。

所有几何、配色、每行字数都来自：
  main/qpq_ui.h     版式预算、配色（含原始十六进制注释）
  main/qpq_ui.c     页面卡/顶栏/底栏/列表行/面板的真实画法
  main/qpq_page_*.c 每一页的元素坐标与文案
  main/qpq_wrap.c   折行算法（此处逐字复刻，保证断字位置与设备一致）
  main/qpq_text.c   第 1 题的真实内容、评级表
输出 HTML 之后用 Chrome 逐框截图（4.8 倍设备缩放 → 1152x1536）。
"""
from __future__ import annotations

import pathlib
import re

# ---------------------------------------------------------------- 配色 -----
SCREEN, PAPER, PAPER_ALT = "#1c1510", "#f4e8c1", "#e8d8b0"
INK, INK_SOFT, MUTED, LINE = "#2c1810", "#5c3a28", "#8b7355", "#dcc9a3"
CINNABAR, CINNABAR_D, CINNABAR_S = "#c23b22", "#9a2e1a", "#f7e0d4"
HINTINK, GREEN, GREEN_S = "#b8a080", "#2d7d46", "#dcecd8"

# --------------------------------------------------------------- 版式 ------
PAGE_INSET, PAGE_W, PAGE_H, PAGE_RADIUS = 5, 230, 310, 25
BAR_H, HINT_H = 36, 26
BODY_X, BODY_W, BODY_H = 10, 210, 248
BODY_TOP = 36   # qpq_body_create 把正文容器建在卡片 y=36 处；子元素坐标从它算起
ROW_H, ROW_GAP, ROW_BORDER, ROW_PAD, ROW_TAG_W = 36, 4, 3, 8, 26
LINE_HEIGHT = {32: 38, 24: 29, 16: 20}
LINE_SPACE = 4

CLOSING = set("，。！？、；：）」』》】…—")


def wrap(text: str, per_line: int) -> list[str]:
    """逐字复刻 qpq_wrap_utf8：从最宽处往回收，直到下一行首字不是收尾标点。"""
    lines, at = [], 0
    while at < len(text):
        probe = text[at:at + per_line + 1]
        if len(probe) <= per_line:
            take = len(probe)
        else:
            take = per_line
            while take > 1 and probe[take] in CLOSING:
                take -= 1
        lines.append(probe[:take])
        at += take
    return lines


def lines_html(text: str, per_line: int | None, size: int, color: str,
               align: str = "left") -> str:
    """自动折行的文本块（每行一个 div，避免浏览器自己再折一次）。"""
    rows = wrap(text, per_line) if per_line else [text]
    body = []
    for index, row in enumerate(rows):
        last = index == len(rows) - 1
        margin = "" if last else f"margin-bottom:{LINE_SPACE}px;"
        body.append(
            f'<div style="line-height:{LINE_HEIGHT[size]}px;{margin}">{row}</div>')
    return (f'<div style="font-size:{size}px;color:{color};'
            f'text-align:{align};white-space:nowrap;">' + "".join(body) + "</div>")


def abs_box(x: int, y: int, w: int, h: int | None, inner: str,
            top: int = BODY_TOP) -> str:
    """正文区子元素：y 是相对正文容器（卡片 y=36）的，转成卡片坐标。"""
    y = y + top
    height = f"height:{h}px;" if h is not None else ""
    return (f'<div style="position:absolute;left:{x}px;top:{y}px;width:{w}px;'
            f'{height}">{inner}</div>')


def topbar(left: str = "", right: str = "", battery: int | None = None) -> str:
    parts = []
    if left:
        parts.append(
            f'<div style="position:absolute;left:14px;top:0;height:{BAR_H}px;'
            f'display:flex;align-items:center;font-size:16px;line-height:20px;'
            f'color:{PAPER};white-space:nowrap;">{left}</div>')
    if right:
        parts.append(
            f'<div style="position:absolute;right:14px;top:0;height:{BAR_H}px;'
            f'display:flex;align-items:center;font-size:16px;line-height:20px;'
            f'color:{HINTINK};white-space:nowrap;">{right}</div>')
    if battery is not None:
        # 外壳 20x11、四条 2px 实心边、右侧 3x5 极头；电量数字右边缘落在 230-40。
        edge = "".join(
            f'<div style="position:absolute;{css}"></div>' for css in (
                f'left:0;top:0;width:20px;height:2px;background:{HINTINK}',
                f'left:0;bottom:0;width:20px;height:2px;background:{HINTINK}',
                f'left:0;top:0;width:2px;height:11px;background:{HINTINK}',
                f'right:0;top:0;width:2px;height:11px;background:{HINTINK}',
            ))
        usable = 20 - 2 * (2 + 1)
        fill_w = max(1, usable * battery // 100)
        shell = (f'<div style="position:absolute;right:13px;top:12.5px;width:20px;'
                 f'height:11px;">{edge}'
                 f'<div style="position:absolute;left:3px;top:3px;width:{fill_w}px;'
                 f'height:5px;background:{MUTED};border-radius:2px;"></div></div>'
                 f'<div style="position:absolute;right:10px;top:13px;width:3px;'
                 f'height:5px;background:{HINTINK};border-radius:1px;"></div>')
        parts.append(shell)
        parts.append(
            f'<div style="position:absolute;right:40px;top:0;height:{BAR_H}px;'
            f'display:flex;align-items:center;font-size:16px;line-height:20px;'
            f'color:{HINTINK};">{battery}%</div>')
    return (f'<div style="position:absolute;left:0;top:0;width:{PAGE_W}px;'
            f'height:{BAR_H}px;background:{INK};">' + "".join(parts) + "</div>")


def hint(text: str) -> str:
    return (f'<div style="position:absolute;left:0;top:{PAGE_H - HINT_H}px;'
            f'width:{PAGE_W}px;height:{HINT_H}px;background:{INK};display:flex;'
            f'align-items:center;justify-content:center;font-size:16px;'
            f'line-height:20px;color:{HINTINK};white-space:nowrap;">{text}</div>')


def row(x: int, y: int, text: str, tag: str = "", note: str = "",
        state: str = "normal", top: int = BODY_TOP) -> str:
    """列表行：36px 高、圆角 8、边框 3，子元素坐标与 LVGL 的 content 区一致。"""
    y = y + top
    palette = {
        "normal": (PAPER_ALT, LINE, INK, MUTED),
        "selected": (CINNABAR_S, CINNABAR, CINNABAR_D, CINNABAR_D),
        "correct": (GREEN_S, GREEN, GREEN, GREEN),
        "wrong": (CINNABAR_S, CINNABAR, CINNABAR_D, CINNABAR_D),
    }[state]
    bg, border, ink, note_ink = palette
    inner = []
    if tag:
        inner.append(
            f'<div style="position:absolute;left:{ROW_PAD}px;top:0;bottom:0;'
            f'width:{ROW_TAG_W}px;display:flex;align-items:center;font-size:24px;'
            f'line-height:29px;color:{ink};">{tag}</div>')
    left = ROW_PAD + (ROW_TAG_W if tag else 0)
    inner.append(
        f'<div style="position:absolute;left:{left}px;top:0;bottom:0;right:{ROW_PAD}px;'
        f'display:flex;align-items:center;font-size:24px;line-height:29px;'
        f'color:{ink};white-space:nowrap;overflow:hidden;">{text}</div>')
    if note:
        inner.append(
            f'<div style="position:absolute;right:{ROW_PAD}px;top:0;bottom:0;'
            f'display:flex;align-items:center;font-size:16px;line-height:20px;'
            f'color:{note_ink};white-space:nowrap;">{note}</div>')
    return (f'<div style="position:absolute;left:{x}px;top:{y}px;width:{BODY_W}px;'
            f'height:{ROW_H}px;box-sizing:border-box;border:{ROW_BORDER}px solid '
            f'{border};border-radius:8px;background:{bg};">' + "".join(inner) + "</div>")


# ---------------------------------------------------------- 真实题目内容 ----
CATEGORY = "思亲念家"
SENTENCE_SLOT = "自别慈颜，时怀＿＿＿＿。"     # 答题页：槽位形态（qpq_sentence_slot）
SENTENCE_FILLED = "自别慈颜，时怀孺慕。"       # 判卷页：已填空形态（qpq_sentence_filled）
OPTIONS = ["牵挂", "思念", "孺慕", "惦记"]
ANSWER = "孺慕"
EXPLAIN = '"孺慕"指对父母的敬爱和依恋，出自《礼记·檀弓》——侨批惯用典雅书面语表达情感。'
SOURCE = "菲律宾华侨施教布寄给母亲（1928年）"
FULL = "自别慈颜，时怀孺慕。每思禀候，辄乏鸿鳞。"
RANK, SUBTITLE = "番客子弟", "知书达理的华侨后裔"
DESC = "你对侨批有基本的了解，但还有很多侨批中的典故和用法等待你去发现。"
STATS = ["正确 13/20", "最高连对 5 题", "用时 96 秒", "得分 115"]


def page_title() -> str:
    body = [abs_box(BODY_X, 42, BODY_W, None,
                    lines_html("侨批", None, 32, INK, "center")),
            abs_box(BODY_X, 88, BODY_W, None,
                    lines_html("已见 12/91 · 最好 150 分", None, 16, MUTED))]
    for index, (text, note, state) in enumerate(
            [("开始一局", "", "selected"), ("音量", "80%", "normal"),
             ("重置记录", "", "normal")]):
        body.append(row(BODY_X, 124 + index * (ROW_H + ROW_GAP), text,
                        note=note, state=state))
    return topbar("侨批 · 填字问答", battery=82) + "".join(body) + \
        hint("上下选择 · 确定进入")


def page_ask() -> str:
    body = [abs_box(BODY_X, 0, BODY_W, None,
                    lines_html(SENTENCE_SLOT, 8, 24, INK, "center"))]
    tags = "甲乙丙丁"
    for index, text in enumerate(OPTIONS):
        body.append(row(BODY_X, 92 + index * (ROW_H + ROW_GAP), text,
                        tag=tags[index],
                        state="selected" if index == 0 else "normal"))
    return topbar(CATEGORY, "1/20") + "".join(body) + hint("上下选择 · 确定作答")


def page_reveal() -> str:
    items = [
        ("text", "回答错误", 24, CINNABAR, None),
        ("text", f"正确答案：{ANSWER}", 24, CINNABAR_D, None),
        ("text", SENTENCE_FILLED, 24, INK, 8),
        ("text", "解析", 16, MUTED, None),
        ("text", EXPLAIN, 24, INK_SOFT, 8),
        ("text", "完整原文", 16, MUTED, None),
        ("text", FULL, 24, INK, 8),
        ("text", "出处", 16, MUTED, None),
        ("text", SOURCE, 16, INK_SOFT, 13),
    ]
    blocks, y = [], 2  # 滚动容器自身有 2px 上下内边距
    for index, (_, text, size, color, per_line) in enumerate(items):
        rows = wrap(text, per_line) if per_line else [text]
        height = len(rows) * LINE_HEIGHT[size] + (len(rows) - 1) * LINE_SPACE
        blocks.append(abs_box(BODY_X, y, BODY_W, None,
                              lines_html(text, per_line, size, color), top=0))
        y += height + (6 if index != len(items) - 1 else 0)
    scroll = (f'<div style="position:absolute;left:0;top:{BODY_TOP}px;width:{PAGE_W}px;'
              f'height:{BODY_H}px;overflow:hidden;">' + "".join(blocks) + "</div>")
    return topbar() + scroll + hint("上下翻阅 · 确定 下一题")


def page_summary() -> str:
    body = [abs_box(BODY_X, 0, BODY_W, None, lines_html(RANK, None, 32, CINNABAR_D, "center")),
            abs_box(BODY_X, 44, BODY_W, None, lines_html(SUBTITLE, None, 16, MUTED, "center"))]
    for index, text in enumerate(STATS):
        body.append(abs_box(BODY_X, 72 + index * 22, BODY_W, None,
                            lines_html(text, None, 16, INK_SOFT)))
    body.append(abs_box(BODY_X, 168, BODY_W, None, lines_html(DESC, 13, 16, MUTED)))
    return topbar("本局成绩") + "".join(body) + hint("确定 再来一局 · 长按返回")


def frame(caption: str, inner: str) -> str:
    return (f'<div style="width:240px;">'
            f'<div style="font-size:12px;color:#555;margin-bottom:6px;">{caption}</div>'
            f'<div style="position:relative;width:240px;height:320px;background:{SCREEN};'
            f'border-radius:30px;overflow:hidden;">'
            f'<div style="position:absolute;left:{PAGE_INSET}px;top:{PAGE_INSET}px;'
            f'width:{PAGE_W}px;height:{PAGE_H}px;background:{PAPER};'
            f'border-radius:{PAGE_RADIUS}px;overflow:hidden;">{inner}</div></div></div>')


pages = [("标题页 · 开始一局 / 音量 / 重置记录", page_title()),
         ("答题页 · 四选一（甲乙丙丁）", page_ask()),
         ("判卷页 · 可滚动：对错、解析、完整原文、出处", page_reveal()),
         ("结算页 · 评级、四项数据、评语", page_summary())]

html = ('<!DOCTYPE html>\n<html lang="zh-CN">\n<head>\n<meta charset="utf-8">\n'
        '<title>Qiaopi Quiz - four screen designs</title>\n'
        '<!-- 按源码版式常量重绘的四个页面（示意，非真机截图）。'
        '绘制依据：main/qpq_ui.{h,c}、main/qpq_page_*.c、main/qpq_wrap.c、main/qpq_text.c。'
        '每个 240x320 设备框按 4.8 倍设备缩放单独截图，正好得到 1152x1536（竖版 3:4）。'
        '详见 assets/README.md。 -->\n'
        '<style>body{margin:24px;background:#fff;'
        "font-family:'Noto Sans CJK SC','Microsoft YaHei','PingFang SC',sans-serif;}"
        '*{box-sizing:border-box;}</style>\n</head>\n<body>\n'
        '<div style="display:flex;flex-wrap:wrap;gap:18px;">'
        + "".join(frame(caption, inner) for caption, inner in pages)
        + "</div>\n</body>\n</html>\n")

out = pathlib.Path(__file__).with_name("qiaopi-screens.html")
out.write_text(html, encoding="utf-8")
print("写出", out, len(html), "字符")

# ---- 自查：断字位置与纵向累计（必须与设备一致才敢叫"如实"）----
print("\n[断字] 答题页句子（8 字/行）：", wrap(SENTENCE_SLOT, 8))
print("[断字] 判卷页整句（8 字/行）：", wrap(SENTENCE_FILLED, 8))
print("[断字] 解析（8 字/行）：", wrap(EXPLAIN, 8))
print("[断字] 完整原文（8 字/行）：", wrap(FULL, 8))
print("[断字] 出处（13 字/行）：", wrap(SOURCE, 13))
print("[断字] 评语（13 字/行）：", wrap(DESC, 13))

print("\n[纵向] 全部坐标为「设备坐标」（卡片内缩 5 + 正文偏移 36 已计入）")
print("[纵向] 标题页：菜单底边 =",
      124 + 2 * (ROW_H + ROW_GAP) + ROW_H, "（正文区上限 248）")
print("[纵向] 答题页：末行候选底边 =",
      92 + 3 * (ROW_H + ROW_GAP) + ROW_H, "（正文区上限 248）")
y = 2
items = [("回答错误", 24, None), (f"正确答案：{ANSWER}", 24, None),
         (SENTENCE_FILLED, 24, 8), ("解析", 16, None), (EXPLAIN, 24, 8),
         ("完整原文", 16, None), (FULL, 24, 8), ("出处", 16, None), (SOURCE, 16, 13)]
for index, (text, size, per_line) in enumerate(items):
    rows = wrap(text, per_line) if per_line else [text]
    height = len(rows) * LINE_HEIGHT[size] + (len(rows) - 1) * LINE_SPACE
    print(f"[纵向] 判卷页 {index} {text[:8]!r}: {len(rows)} 行 / {height}px  y={y}"
          f"{'  <-- 落在 248 之外，本该滚动' if y + height > 248 else ''}")
    y += height + (6 if index != len(items) - 1 else 0)
