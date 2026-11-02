#!/usr/bin/env python3
"""scripts/gen_assets.py — генератор растровых ассетов вертикального среза.

Создает PNG-файлы по манифесту assets/textures/tokens.manifest.json:
токены 32×32 (файтер/рог/гоблин), плитки 32×32 (пол/стена/щебень),
иконки состояний 24×24, портреты 64×64. Все изображения строятся
процедурно (детерминированно, сид 20251102) — внешние пакеты не нужны.

Состав и происхождение графики описаны в docs/assets.md.
Запуск:  python3 scripts/gen_assets.py [--root assets] [--preview]
"""
from __future__ import annotations

import argparse
import os
import random
import struct
import zlib

SEED = 20251102


# --- PNG-энкодер (RGBA, 8 бит, filter 0) --------------------------------------
def write_png(path: str, w: int, h: int, px: bytearray) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    raw = b"".join(b"\x00" + bytes(px[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


class Canvas:
    def __init__(self, w: int, h: int):
        self.w, self.h = w, h
        self.px = bytearray(w * h * 4)

    def set(self, x: int, y: int, rgba):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 4
            self.px[i:i + 4] = bytes(rgba)

    def rect(self, x0, y0, x1, y1, rgba):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, rgba)

    def circle(self, cx, cy, r, rgba):
        for y in range(cy - r - 1, cy + r + 2):
            for x in range(cx - r - 1, cx + r + 2):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    self.set(x, y, rgba)

    def noise(self, rng, rgba, count, x0=0, y0=0, x1=None, y1=None):
        x1 = self.w - 1 if x1 is None else x1
        y1 = self.h - 1 if y1 is None else y1
        for _ in range(count):
            self.set(rng.randint(x0, x1), rng.randint(y0, y1), rgba)


# --- Токены 32×32 по символьным картам -----------------------------------------
PAL_COMMON = {
    ".": (0, 0, 0, 0),        "o": (18, 20, 26, 255),      # контур
    "s": (226, 176, 130, 255), "S": (186, 136, 96, 255),   # кожа / тень кожи
}

FIGHTER_MAP = [
    "................................",
    "................................",
    "...............rr...............",
    "..............rrrr..............",
    "............hhhhhhhh............",
    "...........hhmmmmmmhh...........",
    "...........hmmmmmmmmh...........",
    "...........hmkmmmmkmh...........",
    "...........hmmmmmmmmh...........",
    "............hmmSSmmh............",
    ".............ssssss.............",
    "..........aaaassaaaaaa..........",
    ".........aabbaaaaabbaaa.........",
    ".........abbbbaaaabbba..ww......",
    ".........abbbbaaaabbba..ww......",
    ".........abssaaaaassa...ww......",
    ".........abssaaaaassaa..ww......",
    ".........aassaaaaassaa..ww......",
    "..........ssaaaaaass....ww......",
    "..........ssaa.aass.....ww......",
    ".........bbb.....bbb....gg......",
    ".........bbb.....bbb............",
    "........bbbb.....bbbb...........",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
]
FIGHTER_PAL = {**PAL_COMMON,
    "h": (168, 176, 190, 255), "m": (140, 148, 164, 255), "k": (40, 46, 60, 255),
    "r": (196, 58, 50, 255), "a": (122, 132, 150, 255), "b": (98, 106, 124, 255),
    "w": (214, 220, 230, 255), "g": (196, 162, 74, 255)}

ROGUE_MAP = [
    "................................",
    "................................",
    "................................",
    "..............dddd..............",
    "............dddddddd............",
    "...........ddwwwwwwdd...........",
    "...........dwkkkkkkwd...........",
    "...........dwkekkekwd...........",
    "...........dwkkkkkkwd...........",
    "............dwwSSwwd............",
    ".............dddddd.............",
    "..........llllllllllll..........",
    ".........lllllllllllll..........",
    ".........lllllllllllll..cc......",
    ".........lllllllllllll..cc......",
    ".........lllssllllssll..cc......",
    ".........lllssllllssll..cc......",
    ".........llllllllllll...cc......",
    "..........lllllllll.....cc......",
    "..........llll.llll.....cc......",
    ".........ddd.....ddd....cc......",
    ".........ddd.....ddd............",
    "........dddd.....dddd...........",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
]
ROGUE_PAL = {**PAL_COMMON,
    "d": (58, 48, 64, 255), "w": (40, 32, 46, 255), "k": (26, 22, 30, 255),
    "e": (228, 214, 120, 255), "l": (76, 64, 84, 255), "c": (170, 178, 190, 255)}

GOBLIN_MAP = [
    "................................",
    "................................",
    "................................",
    "................................",
    "..........e..........e..........",
    "..........ee........ee..........",
    "...........egggggggge...........",
    "...........gggggggggg...........",
    "...........ggrggggrgg...........",
    "...........gggggggggg...........",
    "............ggttttgg............",
    ".............gggggg.............",
    ".............bbbbbb.............",
    "..........bbbbbbbbbbbb..........",
    ".........bbbggbbbbgggbb.........",
    ".........bbgggbbbbgggbb.........",
    ".........bbgggbbbbgggbb.........",
    "..........bggbbbbbbggb..........",
    "..........bgggbbbbgggb..........",
    "...........gg.bbbb.gg...........",
    "..........gg...bb...gg..........",
    ".........ff.....bb....ff........",
    ".........ff....bbbb...ff........",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
]
GOBLIN_PAL = {**PAL_COMMON,
    "g": (106, 168, 78, 255), "e": (78, 132, 56, 255), "r": (228, 64, 50, 255),
    "t": (240, 236, 214, 255), "b": (124, 92, 56, 255), "f": (92, 66, 40, 255)}


def draw_map_sprite(cmap, pal, size=32):
    c = Canvas(size, size)
    for y, row in enumerate(cmap):
        for x, ch in enumerate(row):
            c.set(x, y, pal[ch])
    return c


# --- Плитки 32×32 ---------------------------------------------------------------
def tile_floor(rng):
    c = Canvas(32, 32)
    c.rect(0, 0, 31, 31, (88, 84, 96, 255))
    c.rect(0, 0, 14, 14, (100, 96, 108, 255))
    c.rect(16, 16, 31, 31, (100, 96, 108, 255))
    c.rect(16, 0, 31, 14, (94, 90, 102, 255))
    c.rect(0, 16, 14, 31, (94, 90, 102, 255))
    c.noise(rng, (112, 108, 120, 255), 40)
    c.noise(rng, (78, 74, 86, 255), 30)
    return c


def tile_wall(rng):
    c = Canvas(32, 32)
    c.rect(0, 0, 31, 31, (58, 58, 70, 255))
    for row in range(4):
        y0 = row * 8
        off = 0 if row % 2 == 0 else 8
        for col in range(-1, 4):
            x0 = col * 16 + off
            c.rect(max(0, x0 + 1), y0 + 1, min(31, x0 + 14), y0 + 6, (76, 76, 90, 255))
    c.noise(rng, (86, 86, 100, 255), 30)
    c.noise(rng, (48, 48, 60, 255), 30)
    return c


def tile_rubble(rng):
    c = tile_floor(rng)
    for _ in range(9):
        x, y = rng.randint(2, 27), rng.randint(2, 27)
        r = rng.randint(1, 3)
        shade = rng.randint(96, 132)
        c.circle(x, y, r, (shade, shade - 6, shade - 14, 255))
    c.noise(rng, (70, 66, 76, 255), 40)
    return c


# --- Иконки состояний 24×24 ------------------------------------------------------
def icon_advantage():
    c = Canvas(24, 24)
    for i, shade in enumerate([(60, 190, 90, 255), (40, 150, 70, 255)]):
        for y in range(4, 16):
            half = y - 3
            c.rect(12 - half, y + i, 12 + half, y + i, shade)
    c.rect(10, 16, 14, 21, (40, 150, 70, 255))
    return c


def icon_disadvantage():
    c = Canvas(24, 24)
    for i, shade in enumerate([(220, 80, 60, 255), (170, 56, 42, 255)]):
        for y in range(4, 16):
            half = 15 - y
            c.rect(12 - half, y + i, 12 + half, y + i, shade)
    c.rect(10, 2, 14, 7, (170, 56, 42, 255))
    return c


def icon_prone():
    c = Canvas(24, 24)
    c.circle(5, 14, 3, (226, 176, 130, 255))          # голова
    c.rect(8, 13, 18, 17, (122, 132, 150, 255))        # тело лёжа
    c.rect(17, 15, 22, 17, (98, 106, 124, 255))        # ноги
    for x in (9, 13, 17):                              # линии движения
        c.rect(x, 6, x + 3, 6, (200, 200, 210, 255))
        c.rect(x + 1, 9, x + 3, 9, (160, 160, 172, 255))
    return c


# --- Портреты 64×64 ---------------------------------------------------------------
def portrait_background(c, rng, top, bottom):
    for y in range(64):
        t = y / 63.0
        col = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        c.rect(0, y, 63, y, col)
    c.noise(rng, (255, 255, 255, 8), 40)


def portrait_fighter(rng):
    c = Canvas(64, 64)
    portrait_background(c, rng, (52, 44, 40), (24, 20, 26))
    c.rect(10, 52, 53, 63, (122, 132, 150, 255))        # плечи-кираса
    c.rect(12, 50, 51, 53, (150, 160, 178, 255))
    c.rect(26, 44, 37, 53, (226, 176, 130, 255))        # шея
    c.circle(32, 32, 15, (168, 176, 190, 255))          # шлем
    c.rect(17, 30, 46, 42, (140, 148, 164, 255))        # нижняя часть шлема
    c.rect(20, 32, 43, 38, (40, 46, 60, 255))           # смотровая щель
    c.set(26, 35, (228, 214, 120, 255)); c.set(27, 35, (228, 214, 120, 255))
    c.set(37, 35, (228, 214, 120, 255)); c.set(38, 35, (228, 214, 120, 255))
    c.rect(29, 8, 34, 18, (196, 58, 50, 255))           # плюмаж
    c.circle(32, 8, 3, (220, 80, 60, 255))
    c.rect(17, 26, 46, 27, (200, 206, 218, 255))        # блик шлема
    return c


def portrait_rogue(rng):
    c = Canvas(64, 64)
    portrait_background(c, rng, (36, 32, 44), (16, 14, 22))
    c.rect(12, 52, 51, 63, (58, 48, 64, 255))           # плечи плаща
    c.circle(32, 30, 17, (58, 48, 64, 255))             # капюшон
    c.circle(32, 33, 12, (26, 22, 30, 255))             # тень лица
    c.rect(24, 38, 40, 44, (186, 136, 96, 255))         # нижняя часть лица
    c.set(26, 33, (228, 214, 120, 255)); c.set(27, 33, (228, 214, 120, 255))
    c.set(37, 33, (228, 214, 120, 255)); c.set(38, 33, (228, 214, 120, 255))
    c.rect(16, 20, 20, 26, (76, 64, 84, 255))           # складки капюшона
    c.rect(44, 20, 48, 26, (76, 64, 84, 255))
    c.rect(46, 48, 50, 60, (170, 178, 190, 255))        # кинжал за плечом
    return c


def portrait_goblin(rng):
    c = Canvas(64, 64)
    portrait_background(c, rng, (40, 46, 32), (18, 22, 16))
    c.rect(16, 54, 47, 63, (124, 92, 56, 255))          # плечи
    c.circle(32, 34, 16, (106, 168, 78, 255))           # голова
    for sx in (-1, 1):                                   # уши
        for i in range(10):
            c.rect(32 + sx * (14 + i), 30 - i // 2, 32 + sx * (15 + i), 36 + i // 3,
                   (94, 152, 68, 255))
    c.circle(25, 31, 3, (228, 64, 50, 255))              # глаза
    c.circle(39, 31, 3, (228, 64, 50, 255))
    c.set(25, 30, (255, 220, 160, 255)); c.set(39, 30, (255, 220, 160, 255))
    c.rect(24, 42, 40, 45, (60, 96, 44, 255))            # пасть
    for x in range(25, 40, 3):                           # зубы
        c.rect(x, 42, x + 1, 44, (240, 236, 214, 255))
    c.rect(26, 22, 30, 26, (94, 152, 68, 255))           # бородавки
    return c


# --- Сборка и запись -------------------------------------------------------------
def generate(root: str, preview: bool) -> None:
    rng = random.Random(SEED)
    tex = os.path.join(root, "textures")

    sprites = {
        "tokens/fighter.png": draw_map_sprite(FIGHTER_MAP, FIGHTER_PAL),
        "tokens/rogue.png": draw_map_sprite(ROGUE_MAP, ROGUE_PAL),
        "tokens/goblin.png": draw_map_sprite(GOBLIN_MAP, GOBLIN_PAL),
        "tiles/floor_stone.png": tile_floor(rng),
        "tiles/wall_stone.png": tile_wall(rng),
        "tiles/rubble.png": tile_rubble(rng),
        "icons/advantage.png": icon_advantage(),
        "icons/disadvantage.png": icon_disadvantage(),
        "icons/prone.png": icon_prone(),
        "portraits/fighter.png": portrait_fighter(rng),
        "portraits/rogue.png": portrait_rogue(rng),
        "portraits/goblin.png": portrait_goblin(rng),
    }
    for rel, canvas in sprites.items():
        path = os.path.join(tex, rel)
        write_png(path, canvas.w, canvas.h, canvas.px)
        print(f"  {path}  {canvas.w}×{canvas.h}")

    if preview:
        # Монтаж: токены и плитки 32×32, иконки 24×24, портреты 64×64 — на одном листе
        sheet = Canvas(352, 176)
        sheet.rect(0, 0, 351, 175, (30, 30, 38, 255))
        order32 = ["tokens/fighter.png", "tokens/rogue.png", "tokens/goblin.png",
                   "tiles/floor_stone.png", "tiles/wall_stone.png", "tiles/rubble.png"]
        for i, rel in enumerate(order32):
            blit(sheet, sprites[rel], 8 + i * 40, 8)
        order24 = ["icons/advantage.png", "icons/disadvantage.png", "icons/prone.png"]
        for i, rel in enumerate(order24):
            blit(sheet, sprites[rel], 8 + i * 32, 56)
        order64 = ["portraits/fighter.png", "portraits/rogue.png", "portraits/goblin.png"]
        for i, rel in enumerate(order64):
            blit(sheet, sprites[rel], 8 + i * 112, 88)
        out = os.path.join(tex, "preview_sheet.png")
        write_png(out, sheet.w, sheet.h, sheet.px)
        print(f"  {out}  {sheet.w}×{sheet.h}")


def blit(dst: Canvas, src: Canvas, dx: int, dy: int) -> None:
    for y in range(src.h):
        for x in range(src.w):
            i = (y * src.w + x) * 4
            if src.px[i + 3] > 0:
                dst.set(dx + x, dy + y, src.px[i:i + 4])


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default="assets")
    ap.add_argument("--preview", action="store_true",
                    help="собрать лист-превью textures/preview_sheet.png")
    args = ap.parse_args()
    print("Генерация ассетов (сид %d):" % SEED)
    generate(args.root, args.preview)
