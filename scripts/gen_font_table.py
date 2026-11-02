#!/usr/bin/env python3
# scripts/gen_font_table.py — генерация engine/render/font_table.inc
# из таблицы штрихов scripts/font_glyph_strokes.py (детерминированный вывод, NFR-E09).
# Запуск: python3 scripts/gen_font_table.py > engine/render/font_table.inc
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from font_glyph_strokes import build_glyph_map

DESIGN_MIN_Y = -3.0
DESIGN_MAX_Y = 10.0
DESIGN_WIDTH = 6.0

def main():
    glyphs = build_glyph_map()
    print("// engine/render/font_table.inc — ТАБЛИЦА СГЕНЕРИРОВАНА, НЕ РЕДАКТИРОВАТЬ ВРУЧНУЮ.")
    print("// Источник: scripts/font_glyph_strokes.py, генератор: scripts/gen_font_table.py")
    print("// Формат: {codepoint, количество отрезков, {x0,y0,x1,y1, ...}} — дизайн-сетка 6x10,")
    print("//         y < 0 — диакритика над знаком, y > 8 — выносные элементы.")
    print("struct GlyphDef { unsigned int code; unsigned char segments; const float* pts; };")
    print()
    arrays = []
    entries = []
    for code in sorted(glyphs):
        segs = glyphs[code]
        if not segs:
            entries.append(f"    {{{code}, 0, nullptr}},")
            continue
        name = f"g{code:04X}"
        flat = []
        for s in segs:
            assert len(s) == 4, s
            flat.extend(float(v) for v in s)
        arrays.append("static const float %s[] = {%s};" % (name, ", ".join("%g" % v for v in flat)))
        entries.append(f"    {{{code}, {len(segs)}, {name}}},")
    print("namespace {")
    print()
    for a in arrays:
        print(a)
    print()
    print("const GlyphDef kGlyphDefs[] = {")
    for e in entries:
        print(e)
    print("};")
    print()
    print("constexpr std::size_t kGlyphDefCount = sizeof(kGlyphDefs) / sizeof(kGlyphDefs[0]);")
    print()
    print(f"constexpr float kDesignMinY = {DESIGN_MIN_Y}f;")
    print(f"constexpr float kDesignMaxY = {DESIGN_MAX_Y}f;")
    print(f"constexpr float kDesignWidth = {DESIGN_WIDTH}f;")
    print()
    print("} // namespace")

if __name__ == '__main__':
    main()
