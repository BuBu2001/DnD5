#!/usr/bin/env python3
"""scripts/gen_audio.py — генератор звуковых ассетов (SDL2_mixer, FR-E50…E52).

Создаёт WAV-файлы (16 бит, моно/стерео) процедурно — без внешних библиотек:
  ui_click.wav     — клик интерфейса (короткий «блип» 880 Гц)
  attack_hit.wav   — попадание (удар + шум)
  attack_miss.wav  — промах (свип вниз)
  step.wav         — шаг по клетке (короткий стук)
  battle_theme.ogg → вместо него battle_theme.wav — тема боя (минорный рифф,
                     8 секунд, зацикливается через SDL_mixer loop)

Детерминированно (сид 20251102). Состав и происхождение — docs/assets.md.
Запуск:  python3 scripts/gen_audio.py [--root assets]
"""
from __future__ import annotations

import argparse
import math
import os
import random
import struct
import wave

SEED = 20251102
RATE = 22050


def write_wav(path: str, samples, rate: int = RATE, channels: int = 1) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = bytearray()
        for s in samples:
            v = max(-1.0, min(1.0, s))
            frames += struct.pack("<h", int(v * 32767))
        w.writeframes(bytes(frames))
    print(f"  {path}  {len(samples)/rate:.2f} c, {channels} кан.")


def env(i: int, n: int, attack: float = 0.01, release: float = 0.5) -> float:
    """Огибающая: быстрая атака, экспоненциальный спад."""
    t = i / n
    a = min(1.0, t / max(attack, 1e-6))
    r = math.exp(-release * t * 6.0)
    return a * r


def tone(freq: float, dur: float, release: float = 0.6, vol: float = 0.6):
    n = int(RATE * dur)
    return [vol * env(i, n, release=release) *
            math.sin(2 * math.pi * freq * i / RATE) for i in range(n)]


def noise(dur: float, vol: float = 0.4, rng=None):
    rng = rng or random.Random(SEED)
    n = int(RATE * dur)
    return [vol * env(i, n, release=1.2) * rng.uniform(-1, 1) for i in range(n)]


def sweep(f0: float, f1: float, dur: float, vol: float = 0.5):
    n = int(RATE * dur)
    out = []
    phase = 0.0
    for i in range(n):
        f = f0 + (f1 - f0) * (i / n)
        phase += 2 * math.pi * f / RATE
        out.append(vol * env(i, n, release=0.8) * math.sin(phase))
    return out


def mix(a, b):
    n = max(len(a), len(b))
    return [(a[i] if i < len(a) else 0.0) + (b[i] if i < len(b) else 0.0)
            for i in range(n)]


def battle_theme(dur: float = 8.0):
    """Тема боя: минорный рифф (A2/E3/A3) + бас-пульс + лёгкий шумовой «барабан»."""
    rng = random.Random(SEED)
    n = int(RATE * dur)
    out = [0.0] * n
    beat = RATE // 2  # 120 BPM
    riff = [220.0, 220.0, 329.63, 220.0, 261.63, 220.0, 196.0, 220.0]  # A3 A3 E4 A3 C4 A3 G3 A3
    bass = [110.0, 110.0, 82.41, 110.0]  # A2 A2 E2 A2
    for b in range(n // beat + 1):
        start = b * beat
        if start >= n:
            break
        # рифф (короткие ноты)
        f = riff[b % len(riff)]
        length = min(beat - RATE // 16, n - start)
        for i in range(length):
            t = i / length
            a = min(1.0, i / (RATE * 0.01)) * math.exp(-3.0 * t)
            out[start + i] += 0.22 * a * (math.sin(2 * math.pi * f * i / RATE)
                                          + 0.4 * math.sin(4 * math.pi * f * i / RATE))
        # бас каждый второй бит
        if b % 2 == 0:
            fb = bass[(b // 2) % len(bass)]
            for i in range(min(beat * 2, n - start)):
                t = i / (beat * 2)
                out[start + i] += 0.30 * math.exp(-2.2 * t) * math.sin(2 * math.pi * fb * i / RATE)
        # «бочка» — короткий низкий удар на каждый бит
        for i in range(min(RATE // 10, n - start)):
            t = i / (RATE // 10)
            out[start + i] += 0.35 * math.exp(-8 * t) * math.sin(2 * math.pi * (70 - 30 * t) * i / RATE)
    # мягкое затухание концов петли (бесшовный loop)
    fade = RATE // 20
    for i in range(fade):
        out[i] *= i / fade
        out[n - 1 - i] *= i / fade
    return out


def generate(root: str) -> None:
    audio = os.path.join(root, "audio")
    write_wav(f"{audio}/ui_click.wav", tone(880.0, 0.07, release=1.5, vol=0.5))
    write_wav(f"{audio}/attack_hit.wav",
              mix(tone(180.0, 0.18, release=1.2, vol=0.7), noise(0.14, 0.45)))
    write_wav(f"{audio}/attack_miss.wav", sweep(700.0, 180.0, 0.18, vol=0.4))
    write_wav(f"{audio}/step.wav",
              mix(tone(120.0, 0.06, release=2.0, vol=0.5), noise(0.05, 0.2)))
    write_wav(f"{audio}/battle_theme.wav", battle_theme(8.0))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default="assets")
    args = ap.parse_args()
    print(f"Генерация звука (сид {SEED}, {RATE} Гц):")
    generate(args.root)
