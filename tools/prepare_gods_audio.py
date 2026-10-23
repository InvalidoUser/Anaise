"""Rebuild the embedded CC0 hymn. Requires soundfile, numpy and scipy offline tools."""
from pathlib import Path
from urllib.request import urlretrieve
import tempfile
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

SOURCE = "https://opengameart.org/sites/default/files/2-hymn_16-bit-original_0.mp3"
with tempfile.TemporaryDirectory() as temp:
    source = Path(temp) / "hymn.mp3"
    urlretrieve(SOURCE, source)
    audio, rate = sf.read(source, always_2d=True)
    audio = resample_poly(audio.mean(axis=1), 22050, rate)[:22050 * 40]
    # Fold the tail into the head for a continuous, click-free phrase.
    seam = 22050
    ramp = np.linspace(0, 1, seam)
    audio[:seam] = audio[-seam:] * (1 - ramp) + audio[:seam] * ramp
    audio = audio[:-seam]
    audio *= 0.72 / max(0.001, np.max(np.abs(audio)))
    destination = Path(__file__).resolve().parents[1] / "assets/audio/hymn.wav"
    sf.write(destination, audio, 22050, subtype="PCM_16")
    print(f"Prepared {len(audio) / 22050:.1f}s CC0 hymn")
