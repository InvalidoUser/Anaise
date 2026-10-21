---
name: Anaise
description: Work on the Anaise black-and-white Win32 C++ app (src/main.cpp, ring pattern, headless renderer). Use for any change, test, capture, or question in this repository. Load this before reading src/main.cpp so the map, invariants and commands are known up front.
---

# Anaise

Read `references/PROJECT_MAP.md` first. It is one page: files, how `main.cpp` is
laid out, screen flow, rendering rules, headless flags, test coverage, and a
generated index with line numbers for every section, type and method. Then open
only the region you need with `offset`/`limit` — never dump all of
`src/main.cpp` (about 3.3k lines, ~130 KB).

## Commands

```bat
build.bat
powershell -ExecutionPolicy Bypass -File tools\test.ps1
powershell -ExecutionPolicy Bypass -File tools\test.ps1 -NoBuild
powershell -ExecutionPolicy Bypass -File tools\headless.ps1 -Screen menu -Size 1080x740 -TimeMs 4200
bin\Anaise.exe --headless --screen all --time 4200 --out build\headless
```

Never launch `bin\Anaise.exe` without `--headless`: the window appears on the
desktop and steals focus. `tools\capture.ps1` is only for a window that is
already running.

## Invariants

- Palette is exactly `#000000` and `#FFFFFF`. Softness comes from ordered
  dithering (Bayer matrix in the ring pattern, thresholded blur for the pilgrim
  glow). Never add grey, alpha to grey, or colour.
- Nothing is written to disk except the PNGs a headless run asks for. No saved
  state, no network, no new dependencies, static CRT.
- Every screen after the splash is composited over the ring background; panels,
  the room and the canvas paint solid black over it.
- Headless output must stay deterministic: with `--headless` there is no timer
  and `nowMs()` is the virtual clock, so a fixed `--time` must give the same PNG.
- The repository is public. No secrets, no personal data, no generated build
  output. Commit and push only when asked.

## Where changes usually go

| Change | Touch |
| --- | --- |
| New screen | `Screen` enum + `screenName()`, `parseHeadlessOptions`, `paintInto` dispatch, `openScreen`, `handleKey`, `handleMessage` (keys, cursor, WM_PAINT) |
| New menu item | `kMenuItems`, `kMenuTitles/Kinds/Blurbs/Hints`, `appIconRows`, `activateMenuItem`, `test.ps1` expectations |
| New dither/glow effect | `App` drawing group, fed into the existing `dither()`-style threshold so output stays binary |
| New headless flag | `HeadlessOptions`, `parseHeadlessOptions`, usage text, and a `Check` in `tools/test.ps1` |
| Ring pattern | `src/ring_pattern.h` (`RingBackground::draw` -> `buildGeometry` -> `addCircle`/`addChord` -> `paintField` -> `dither`) |
| Splash art | `src/angel_art.h` (Braille data) and `DotArt`/`seraphimArt()` |

After editing code, run `powershell -ExecutionPolicy Bypass -File tools\test.ps1`.

## Line numbers drift

`src/main.cpp` changes often, so the numbers in the generated index can be
stale. Grep the anchor text before trusting one. Regenerate the index after a
structural change:

```bat
powershell -ExecutionPolicy Bypass -File .opencode\skills\anaise\scripts\map.ps1
```

## Code graph (graphify)

`graphifyy` (command `graphify`) is installed locally, offline and free: code is
parsed with tree-sitter, no API key, no upload. The graph lives in
`graphify-out/` and is gitignored. If the binary is not on `PATH`, use
`%APPDATA%\Python\Python313\Scripts\graphify.exe` or `python -m graphify`.

```bat
graphify extract . --code-only      :: build graph.json (only if missing)
graphify update .                   :: re-extract after code changes, no LLM
graphify explain "bendRoom"         :: node, file:line, neighbours, edge types
graphify affected "RingBackground::draw" --depth 2
graphify god-nodes --top 10
graphify query "how is the walker lit" --budget 800
```

This repository is small, so use the graph for `explain` / `affected` / `path`
on a named symbol rather than prose questions; the map above is cheaper for
orientation. Graphify prints warnings to stderr and the CLI can exit non-zero
after a successful run — check the files, not just the exit code. Semantic
(docs/PDF) passes need a model and are not used here; keep to `--code-only`.
