"""Parse the Dao De Jing content sources under ``tools/daodejing/chapters/``.

One file per chapter, named ``NNN.txt`` (chapter number, zero padded). The
format is deliberately boring so that every failure is a loud one: an unknown
key, a missing section, a fourth option, or an over-long point all abort the
generator instead of quietly shipping wrong content.

    # comments start with '#' and may appear anywhere
    chapter = 1
    volume = 道经
    title = 道可道

    [原文]
    one reading screen per line

    [点拨]
    one point per line, at most POINT_MAX_CHARS characters

    [参究]
    question = one line
    option   = exactly three, in slot order

The three options are positional: slot 0 means "this landed", slot 1 means
"still chewing on it", slot 2 means "did not connect". Review scheduling is
driven by the slot, so the order must not be shuffled -- that is why the file
format enforces exactly three and the generator emits them unchanged.

Both ``gen_content.py`` (the C tables) and ``gen_font.py`` (the glyph
inventory) import this module, so the tables and the font subset can never
disagree about what the text says.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHAPTERS_DIR = ROOT / "tools" / "daodejing" / "chapters"

# The complete book. Progress is stored for all 81 chapters from day one, even
# while only a prefix of them has curated content.
TOTAL_CHAPTERS = 81

DAODEJING_VOLUMES = ("道经", "德经")
DAO_VOLUME_LAST_CHAPTER = 37

# One point must fit a single screen of the 240x320 panel.
POINT_MAX_CHARS = 60
MIN_POINTS = 3
MAX_POINTS = 5
OPTION_SLOTS = 3

METADATA_KEYS = ("chapter", "volume", "title")
SECTIONS = ("原文", "点拨", "参究")


class ContentError(Exception):
    """Raised for any malformed content source."""


@dataclass(frozen=True)
class Chapter:
    number: int
    volume: str
    title: str
    passages: tuple[str, ...]
    points: tuple[str, ...]
    question: str
    options: tuple[str, ...]

    @property
    def is_dao_volume(self) -> bool:
        return self.volume == DAODEJING_VOLUMES[0]


def volume_for(number: int) -> str:
    return DAODEJING_VOLUMES[0] if number <= DAO_VOLUME_LAST_CHAPTER else DAODEJING_VOLUMES[1]


def _parse(path: Path) -> Chapter:
    metadata: dict[str, str] = {}
    sections: dict[str, list[str]] = {name: [] for name in SECTIONS}
    question = ""
    options: list[str] = []
    current: str | None = None

    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue

        if line.startswith("[") and line.endswith("]"):
            name = line[1:-1].strip()
            if name not in SECTIONS:
                raise ContentError(f"{path.name}:{lineno}: unknown section [{name}]")
            current = name
            continue

        if current is None:
            if "=" not in line:
                raise ContentError(f"{path.name}:{lineno}: expected 'key = value': {line!r}")
            key, _, value = line.partition("=")
            key = key.strip()
            if key not in METADATA_KEYS:
                raise ContentError(f"{path.name}:{lineno}: unknown key {key!r}")
            if key in metadata:
                raise ContentError(f"{path.name}:{lineno}: duplicate key {key!r}")
            metadata[key] = value.strip()
            continue

        if current == "参究":
            key, sep, value = line.partition("=")
            key = key.strip()
            if not sep:
                raise ContentError(f"{path.name}:{lineno}: expected 'key = value': {line!r}")
            if key == "question":
                if question:
                    raise ContentError(f"{path.name}:{lineno}: duplicate question")
                question = value.strip()
            elif key == "option":
                options.append(value.strip())
            else:
                raise ContentError(f"{path.name}:{lineno}: unknown key {key!r}")
            continue

        sections[current].append(line)

    for key in METADATA_KEYS:
        if key not in metadata:
            raise ContentError(f"{path.name}: missing {key} = ...")

    try:
        number = int(metadata["chapter"])
    except ValueError:
        raise ContentError(f"{path.name}: chapter must be an integer, got {metadata['chapter']!r}")

    stem = path.stem
    if not stem.isdigit():
        raise ContentError(f"{path.name}: file name must be the chapter number, e.g. 001.txt")
    if int(stem) != number:
        raise ContentError(f"{path.name}: file name says {int(stem)} but chapter = {number}")

    if not 1 <= number <= TOTAL_CHAPTERS:
        raise ContentError(f"{path.name}: chapter {number} is outside 1..{TOTAL_CHAPTERS}")

    volume = metadata["volume"]
    if volume not in DAODEJING_VOLUMES:
        raise ContentError(f"{path.name}: volume must be one of {DAODEJING_VOLUMES}, got {volume!r}")
    expected_volume = volume_for(number)
    if volume != expected_volume:
        raise ContentError(
            f"{path.name}: chapter {number} belongs to {expected_volume}, not {volume}"
        )

    title = metadata["title"]
    if not title:
        raise ContentError(f"{path.name}: title is empty")

    passages = tuple(sections["原文"])
    if not passages:
        raise ContentError(f"{path.name}: [原文] is empty")

    points = tuple(sections["点拨"])
    if not MIN_POINTS <= len(points) <= MAX_POINTS:
        raise ContentError(
            f"{path.name}: [点拨] needs {MIN_POINTS}..{MAX_POINTS} entries, got {len(points)}"
        )
    for index, point in enumerate(points):
        if len(point) > POINT_MAX_CHARS:
            raise ContentError(
                f"{path.name}: point {index + 1} is {len(point)} characters, "
                f"over the {POINT_MAX_CHARS} limit for one screen"
            )

    if not question:
        raise ContentError(f"{path.name}: [参究] is missing question = ...")
    if len(options) != OPTION_SLOTS:
        raise ContentError(
            f"{path.name}: [参究] needs exactly {OPTION_SLOTS} options, got {len(options)}"
        )
    for slot, option in enumerate(options):
        if not option:
            raise ContentError(f"{path.name}: option slot {slot} is empty")

    return Chapter(
        number=number,
        volume=volume,
        title=title,
        passages=passages,
        points=points,
        question=question,
        options=tuple(options),
    )


def load_chapters() -> tuple[Chapter, ...]:
    """Every chapter source, ordered by chapter number, gaps rejected."""
    paths = sorted(CHAPTERS_DIR.glob("*.txt"))
    if not paths:
        raise ContentError(f"no chapter sources found in {CHAPTERS_DIR}")

    chapters = tuple(_parse(path) for path in paths)

    numbers = [chapter.number for chapter in chapters]
    if numbers != list(range(1, len(numbers) + 1)):
        raise ContentError(
            f"chapters must be numbered 1..N without gaps, got {numbers}"
        )
    return chapters


def content_characters() -> set[str]:
    """Every character a reader can see, for the glyph inventory."""
    chars: set[str] = set()
    for chapter in load_chapters():
        chars.update(chapter.title)
        chars.update(chapter.volume)
        for text in chapter.passages:
            chars.update(text)
        for text in chapter.points:
            chars.update(text)
        chars.update(chapter.question)
        for option in chapter.options:
            chars.update(option)
    chars.discard("\n")
    chars.discard("\r")
    chars.discard("\t")
    return chars
