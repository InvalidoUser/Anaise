"""Import the explicitly CC0 college music and two acted scream recordings."""
from io import BytesIO
from pathlib import Path
from urllib.request import Request, urlopen
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[1] / "assets/audio"
SOURCES = {
    "school": "https://opengameart.org/sites/default/files/Cakeflaps%20-%20Cate.wav",
    "school-scream-1": "https://cdn.freesound.org/previews/169/169811_1122547-hq.mp3",
    "school-scream-2": "https://cdn.freesound.org/previews/169/169810_1122547-hq.mp3",
}
for name, url in SOURCES.items():
    data = urlopen(Request(url, headers={"User-Agent": "Mozilla/5.0"}), timeout=60).read()
    audio, rate = sf.read(BytesIO(data), always_2d=True)
    mono = resample_poly(audio.mean(axis=1), 22050, rate)
    mono *= (0.65 if name == "school" else 0.8) / max(0.001, np.max(np.abs(mono)))
    # Only remove clicks at the loop boundary, preserving the full composition.
    fade = min(220 if name == "school" else 100, len(mono) // 8)
    mono[:fade] *= np.linspace(0, 1, fade)
    mono[-fade:] *= np.linspace(1, 0, fade)
    sf.write(ROOT / f"{name}.wav", mono, 22050, subtype="PCM_16")
    print(f"Prepared {name}: {len(mono) / 22050:.2f}s")
