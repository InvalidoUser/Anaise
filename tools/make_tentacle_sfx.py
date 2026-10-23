"""Synthesise Tentacle's seven sound effects (mono 16-bit, 22050 Hz).

Every sound is generated here from oscillators and noise, so the results are
original work released under CC0 with the rest of the repository.
Run: python tools/make_tentacle_sfx.py
"""
from pathlib import Path
import wave

import numpy as np

RATE = 22050
ROOT = Path(__file__).resolve().parents[1] / "assets" / "audio"
rng = np.random.default_rng(7)


def t(seconds):
    return np.arange(int(RATE * seconds)) / RATE


def env(n, attack=0.004, release=0.6):
    e = np.ones(n)
    a = max(1, int(RATE * attack))
    e[:a] = np.linspace(0, 1, a)
    r = max(1, int(n * release))
    e[-r:] *= np.linspace(1, 0, r) ** 2
    return e


def sweep(f0, f1, seconds, shape="sine"):
    tt = t(seconds)
    f = f0 * (f1 / f0) ** (tt / seconds)
    phase = 2 * np.pi * np.cumsum(f) / RATE
    if shape == "square":
        return np.tanh(np.sin(phase) * 4)
    if shape == "saw":
        return 2 * ((phase / (2 * np.pi)) % 1) - 1
    return np.sin(phase)


def lowpass(x, cutoff):
    a = np.exp(-2 * np.pi * cutoff / RATE)
    y = np.zeros_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def write(name, x, peak=0.8):
    x = x / max(1e-6, np.max(np.abs(x))) * peak
    data = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(str(ROOT / f"tentacle-{name}.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())
    print(f"tentacle-{name}.wav {len(x) / RATE:.2f}s")


# Laser: a bright falling zap with a buzzing edge.
d = 0.14
x = 0.7 * sweep(2400, 520, d, "square") + 0.3 * sweep(4800, 900, d)
write("laser", x * env(len(x), 0.002, 0.8), 0.55)

# Splat: wet noise burst over a dropping thump.
d = 0.22
noise = lowpass(rng.standard_normal(int(RATE * d)), 1800)
thump = sweep(180, 50, d)
x = noise * env(len(noise), 0.001, 0.9) * 1.4 + thump * env(len(thump), 0.002, 0.7)
write("splat", x, 0.75)

# Buy: a gulp, two quick rising blips.
a = sweep(220, 440, 0.08) * env(int(RATE * 0.08), 0.003, 0.5)
b = sweep(330, 880, 0.12) * env(int(RATE * 0.12), 0.003, 0.6)
write("buy", np.concatenate([a, np.zeros(int(RATE * 0.02)), b]), 0.6)

# Wave horn: a low wavering growl that swells and fades.
d = 1.6
tt = t(d)
vib = 1 + 0.02 * np.sin(2 * np.pi * 5.5 * tt)
phase = 2 * np.pi * np.cumsum(55 * vib) / RATE
x = np.tanh(2.5 * (np.sin(phase) + 0.6 * np.sin(1.5 * phase) + 0.3 * (2 * ((phase / np.pi) % 1) - 1)))
swell = np.minimum(1, tt / 0.45) * np.minimum(1, (d - tt) / 0.6)
write("wave", lowpass(x, 900) * swell, 0.85)

# Hurt: a distorted low hit on the mouth.
d = 0.42
x = np.tanh(4 * sweep(120, 38, d)) + 0.5 * lowpass(rng.standard_normal(int(RATE * d)), 600)
write("hurt", x * env(len(x), 0.001, 0.85), 0.85)

# Evolve: a rising four-note arpeggio with a shimmer.
notes = [262, 330, 392, 523]
parts = []
for f in notes:
    tone = np.sin(2 * np.pi * f * t(0.1)) + 0.4 * np.sin(2 * np.pi * f * 2.01 * t(0.1))
    parts.append(tone * env(len(tone), 0.003, 0.5))
write("evolve", np.concatenate(parts), 0.55)

# Pulse: a rushing whoosh ending in a deep boom.
d = 0.9
tt = t(d)
whoosh = lowpass(rng.standard_normal(len(tt)), 2500) * np.sin(np.pi * np.minimum(1, tt / 0.45)) ** 2
boom = sweep(90, 30, d) * (tt > 0.35) * np.exp(-np.maximum(0, tt - 0.35) * 5)
write("pulse", whoosh * 0.8 + np.tanh(3 * boom), 0.9)
