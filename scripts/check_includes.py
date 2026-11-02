#!/usr/bin/env python3
"""scripts/check_includes.py — статическая проверка изоляции слоёв.

Требования ТЗ DOC-04:
  * NFR-E10, п. 4.8.2, E-D01: в каталоге rules/ запрещены включения <SDL.h>,
    <GL/gl.h>, "engine/render/*", "engine/ui/*" и любых заголовков engine/*;
  * E-D02, UI-E01: в engine/ui/* запрещены включения rules/*;
  * E-D01: в engine/* запрещены включения rules/* и game/* (движок не знает правил);
  * E-D11: библиотека rules собирается и тестируется без SDL и OpenGL.

Нарушение направления зависимостей считается дефектом архитектуры (п. 3.3.1)
и блокирует сборку в CI. Выходной код: 0 — нарушений нет, 1 — есть нарушения.
"""

from __future__ import annotations

import os
import re
import sys
from typing import Iterable, List, NamedTuple

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')

HEADER_EXTENSIONS = {".h", ".hpp", ".hxx", ".inl"}
SOURCE_EXTENSIONS = HEADER_EXTENSIONS | {".c", ".cc", ".cpp", ".cxx"}


class Violation(NamedTuple):
    file: str
    line: int
    include: str
    rule: str


def source_files(directory: str) -> Iterable[str]:
    for current, _dirs, files in os.walk(os.path.join(ROOT, directory)):
        for name in sorted(files):
            if os.path.splitext(name)[1].lower() in SOURCE_EXTENSIONS:
                yield os.path.join(current, name)


def includes_of(path: str) -> Iterable[tuple[int, str]]:
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for number, line in enumerate(handle, start=1):
            match = INCLUDE_RE.match(line)
            if match:
                yield number, match.group(1)


def rel(path: str) -> str:
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


def check_rules_isolation() -> List[Violation]:
    """E-D01, E-D11: rules/ не знает ни движка, ни SDL/OpenGL."""
    forbidden_prefixes = ("engine/", "game/")
    forbidden_names = {"SDL.h", "SDL2/SDL.h", "SDL_mixer.h", "SDL2/SDL_mixer.h",
                       "GL/gl.h", "GL/glew.h", "GLES2/gl2.h", "GLES3/gl3.h",
                       "glad/gl.h", "KHR/khrplatform.h"}
    violations: List[Violation] = []
    for path in source_files("rules"):
        for line, include in includes_of(path):
            normalized = include.replace("\\", "/")
            base = os.path.basename(normalized)
            if normalized.startswith(forbidden_prefixes):
                violations.append(Violation(rel(path), line, include,
                                            "E-D01: rules/ не должен включать engine/* или game/*"))
            elif base in forbidden_names or normalized.startswith(("GL/", "GLES", "SDL")):
                violations.append(Violation(rel(path), line, include,
                                            "E-D11: rules/ не должен зависеть от SDL/OpenGL"))
    return violations


def check_engine_isolation() -> List[Violation]:
    """E-D01: engine/ не знает правил D&D и игровой логики."""
    violations: List[Violation] = []
    for path in source_files("engine"):
        for line, include in includes_of(path):
            normalized = include.replace("\\", "/")
            if normalized.startswith(("rules/", "game/")):
                violations.append(Violation(rel(path), line, include,
                                            "E-D01: engine/ не должен включать rules/* или game/*"))
    return violations


def check_ui_isolation() -> List[Violation]:
    """E-D02, UI-E01: слой UI получает только данные и DTO, не правила."""
    forbidden_terms = ("rules/", "AC", "saving_throw", "spell_slot", "sneak_attack", "rage")
    violations: List[Violation] = []
    for path in source_files("engine/ui"):
        for line, include in includes_of(path):
            normalized = include.replace("\\", "/")
            if normalized.startswith(forbidden_terms[:1]) or normalized.startswith("game/"):
                violations.append(Violation(rel(path), line, include,
                                            "E-D02: engine/ui/* не должен включать rules/*"))
    # Дополнительно: запрет D&D-терминов в идентификаторах слоя UI (п. 1.5)
    for path in source_files("engine/ui"):
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for line_number, text in enumerate(handle, start=1):
                if text.lstrip().startswith("//"):
                    continue  # комментарии и ссылки на требования ТЗ допустимы
                lowered = text.lower()
                for term in forbidden_terms[1:]:
                    if re.search(rf"\b{term}\b", lowered):
                        violations.append(Violation(rel(path), line_number, term,
                                                    "п. 1.5: слой UI не должен знать термины D&D"))
    return violations


def check_game_no_formulas() -> List[Violation]:
    """п. 3.3.1: Game Layer не содержит расчётных формул правил, минуя Rules Core.

    Проверяются маркеры хардкода: константа +5 как замена преимущества (AC-K07)
    и самостоятельные формулы модификатора/бонуса мастерства.
    """
    patterns = [
        (re.compile(r"advantage\s*\?\s*5"), "AC-K07: преимущество не заменяют плоским +5"),
        (re.compile(r"\(\s*\w+\s*-\s*10\s*\)\s*/\s*2"), "FR-K11: формула модификатора — только rules::"),
        (re.compile(r"\(\s*level\s*-\s*1\s*\)\s*/\s*4"), "FR-K24: бонус мастерства — только rules::"),
    ]
    violations: List[Violation] = []
    for path in source_files("game"):
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for line_number, text in enumerate(handle, start=1):
                for pattern, rule in patterns:
                    if pattern.search(text):
                        violations.append(Violation(rel(path), line_number, text.strip()[:60], rule))
    return violations


def main() -> int:
    all_violations: List[Violation] = []
    all_violations += check_rules_isolation()
    all_violations += check_engine_isolation()
    all_violations += check_ui_isolation()
    all_violations += check_game_no_formulas()

    if not all_violations:
        print("check_includes: нарушений изоляции слоёв не найдено (NFR-E10, E-D01, E-D02)")
        return 0

    print(f"check_includes: найдено нарушений: {len(all_violations)}")
    for v in all_violations:
        print(f"  {v.file}:{v.line}: #{v.include} — {v.rule}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
