# Anaise

A small black-and-white game library in a native Windows window. Anaise opens
on a dot-art seraphim startup screen, then a main menu with two rooms: an empty
library and a pixel paint canvas.

The palette is strictly black and white, the logo is 10x10 pixel art, and
nothing is written to disk.

## Features

- Pure black (`#000000`) and white (`#FFFFFF`) throughout
- Seraphim startup screen drawn as a field of dots, revealed outward from the
  centre, then the title sputters on like a failing tube
- Main menu with keyboard and mouse navigation
- **The Empty Library**: a quiet room with empty shelves and nothing to do
- **Pixel Paint**: a 32x32 monochrome canvas with brush sizes, erasing, undo and
  clear
- 10x10 pixel-art bookshelf logo, used both in the window and as the app icon
- Titles set in Squealer, a CC0 blackletter typeface embedded in the executable
- Native resizable window, dark title bar, minimise/maximise, snap layouts,
  taskbar icon
- `F11` or `Alt+Enter` true fullscreen, `Esc` to leave it
- `Alt+F4` closes, as a Windows app should
- Per-monitor DPI awareness
- No engine, no runtime bundle, no network access, no saved data

## Startup art

The seraphim is a 114 x 57 Braille drawing (supplied as `angel.txt`) embedded
in `src/angel_art.h`. Each Braille cell encodes a 2 x 4 block of dots, so the
splash decodes the raised dots into a 228 x 228 field, trims the empty border,
and renders every dot as a white dot. The Braille glyphs themselves are never
drawn.

Once the last dot lands the angel holds the screen on its own for a moment, then
the `ANAISE` title ignites like a failing fluorescent tube: a short, fixed
sequence of dark, partly lit and fully lit frames before it stays on. The splash
leaves for the main menu on its own; a key or a click skips it.

## Build

Requires MinGW-w64 (`g++` and `windres`) on `PATH`.

```bat
build.bat
run.bat
```

The executable is written to `bin\Anaise.exe`. It is linked statically against
the C++ runtime, so it needs no MinGW DLLs at run time.

To regenerate the 10x10 icon:

```bat
powershell -ExecutionPolicy Bypass -File tools\make_icon.ps1
```

To put a shortcut to the built app on your desktop:

```bat
powershell -ExecutionPolicy Bypass -File tools\make_shortcut.ps1
```

The shortcut points at `bin\Anaise.exe` and uses the icon embedded in it, so
it keeps working as long as the repository stays where it is.

## Layout

```
src/main.cpp       Win32 window, menu, startup art, library and pixel paint
src/angel_art.h    Braille seraphim source art
src/app.rc         icon, embedded title font, version info
assets/             generated 10x10 pixel-art icon
assets/fonts/       Squealer (CC0 1.0) title typeface and its licence
tools/              icon generator and development helpers
```

## Font

The title uses [Squealer](https://www.dafont.com/squealer.font) by Raymond
Larabie (Typodermic Fonts), released under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). The `.otf` is
compiled into `Anaise.exe` as a resource and registered at startup, so nothing
has to be installed. See `assets/fonts/squealer-LICENSE.txt`.
