"""Download verified CC0 source packs and rebuild six embedded mono PCM effects."""
from io import BytesIO
from pathlib import Path
from urllib.request import urlopen
from zipfile import ZipFile
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[1] / "assets/audio"
PACKS = {
    "impact": "https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip",
    "rpg": "https://kenney.nl/media/pages/assets/rpg-audio/8e99002d76-1677590336/kenney_rpg-audio.zip",
}
packs = {key: ZipFile(BytesIO(urlopen(url, timeout=60).read())) for key, url in PACKS.items()}
for key, pack in packs.items():
    license_text = pack.read("License.txt").decode().replace("\r", "")
    assert "CC0" in license_text
    (ROOT / f"kenney-{key}-CC0.txt").write_text(license_text, encoding="utf-8")

sources = [
    ("spark", packs["rpg"].read("Audio/metalClick.ogg"), 1),
    ("slash", packs["rpg"].read("Audio/knifeSlice.ogg"), .8),
    ("hit", packs["impact"].read("Audio/impactMetal_medium_000.ogg"), .8),
    ("parry", packs["impact"].read("Audio/impactMetal_heavy_000.ogg"), 1),
    ("reveal", urlopen("https://opengameart.org/sites/default/files/ambient_horror.wav", timeout=60).read(), .65),
    ("judgement", packs["impact"].read("Audio/impactBell_heavy_000.ogg"), .45),
]
for name, data, speed in sources:
    audio, rate = sf.read(BytesIO(data), always_2d=True)
    audio = resample_poly(audio.mean(axis=1), 22050, rate)
    audio = np.interp(np.arange(0, len(audio), speed), np.arange(len(audio)), audio)
    audio = audio[:22050 * (6 if name == "reveal" else 4)]
    audio *= .7 / max(.001, np.max(np.abs(audio)))
    fade = min(len(audio) // 4, int(22050 * (.5 if name == "reveal" else .015)))
    audio[:fade] *= np.linspace(0, 1, fade)
    audio[-fade:] *= np.linspace(1, 0, fade)
    sf.write(ROOT / f"gods-{name}.wav", audio, 22050, subtype="PCM_16")
    print(f"Prepared {name}: {len(audio) / 22050:.2f}s")
