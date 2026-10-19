# The Hollow Spire

An ASCII text adventure in a native Windows window.

You are a lamplighter sent up the old pilgrim road to discover why the beacon
of the Hollow Spire has been dark for thirty years. Explore eight rooms, solve
the tower's small mysteries, and decide what to do with the light at the top.

The whole app is a real Win32 desktop program written in C++ with no engine,
no runtime bundle, and no network access. Every letter is drawn with GDI.

## Features

- Native resizable window, menu bar, minimise/maximise, snap layouts, taskbar icon
- `F11` or `Alt+Enter` true fullscreen, `Esc` to leave it
- `Alt+F4` closes, as a Windows app should
- Scrolling story with mouse wheel, Page Up/Down, and a draggable scrollbar
- Command history with Up/Down, Tab completion, click-to-place caret
- Save and load to `%APPDATA%\HollowSpire\chronicle.save`
- Window size, maximised state, and text size remembered
- Per-monitor DPI awareness, dark title bar, drawn scanline-free CRT palette
- Clipboard copy/paste and a built-in command reference
- Nine ASCII room illustrations, four endings, and a small secret or two

## Controls

| Key | Action |
| --- | --- |
| `F1` | Command reference |
| `F2` | New game |
| `F5` / `F9` | Save / load |
| `F11`, `Alt+Enter` | Toggle fullscreen |
| `Esc` | Leave fullscreen, or clear the input line |
| `Alt+F4` | Close the window |
| `Ctrl+C` | Copy the transcript |
| `Ctrl+V` / `Shift+Insert` | Paste |
| `Ctrl+L` | Clear scrollback |
| `Ctrl+Plus` / `Ctrl+Minus` | Larger / smaller text |
| `Page Up/Down` | Scroll the story |
| `Up` / `Down` | Command history |
| `Tab` | Complete the current word |
| Mouse wheel / scrollbar | Scroll the story |

## Build

Requires MinGW-w64 (`g++` and `windres`) on `PATH`.

```bat
build.bat
run.bat
```

The executable is written to `bin\HollowSpire.exe`. It is linked statically
against the C++ runtime, so it needs no MinGW DLLs at run time.

To regenerate the icon:

```bat
powershell -ExecutionPolicy Bypass -File tools\make_icon.ps1
```

## Tests

The story logic has a scripted playthrough test:

```bat
tests\run_tests.bat
```

It uses a temporary APPDATA folder, so running it does not touch your real
save file.

## A gentle hint

The gate, the gatehouse, and the hall are a good first loop. If you find a
bell, ring it where the ghost is. If a cabinet is locked, look downstairs.
The observatory answers to a name, not a command.

## Layout

```
src/main.cpp    Win32 window, GDI renderer, input, menus, fullscreen
src/game.cpp    world, parser, puzzles, story, save/load
src/game.h      shared output and game interface
src/app.rc      icon, version info, manifest
assets/         generated application icon
tools/          icon generator and development helpers
tests/          scripted playthrough test
```
