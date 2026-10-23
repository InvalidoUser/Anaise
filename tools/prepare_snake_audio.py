"""Import three acted CC0 grunts for Snake (development only).

Source/license: https://opengameart.org/content/grunts-male-death-and-pain
The author, thebardofblasphemy, identifies these as recordings of his voice.
"""
from io import BytesIO
from pathlib import Path
from urllib.request import Request, urlopen
import hashlib
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

SOURCE = "https://opengameart.org/sites/default/files/death%20pain%20grunts.wav"
ROOT = Path(__file__).resolve().parents[1] / "assets/audio"
data = urlopen(Request(SOURCE, headers={"User-Agent": "Mozilla/5.0"}), timeout=60).read()
print("Source SHA256:", hashlib.sha256(data).hexdigest())
samples, rate = sf.read(BytesIO(data), always_2d=True)
for number, (start, end) in enumerate(((.40, 1.95), (3.85, 6.20), (10.00, 12.20)), 1):
    mono = samples[int(start * rate):int(end * rate)].mean(axis=1)
    mono = resample_poly(mono, 22050, rate)
    mono *= .75 / max(.001, np.max(np.abs(mono)))
    fade = min(220, len(mono) // 8)
    mono[:fade] *= np.linspace(0, 1, fade)
    mono[-fade:] *= np.linspace(1, 0, fade)
    sf.write(ROOT / f"snake-groan-{number}.wav", mono, 22050, subtype="PCM_16")
    print(f"snake-groan-{number}: {len(mono) / 22050:.2f}s")
