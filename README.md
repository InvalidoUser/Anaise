# Anaise

A small black-and-white game library in a native Windows window. Anaise opens
on a dot-art seraphim startup screen, then a monochrome computer desktop with
five apps to open: Bethelament, a Painter canvas, Veil (a browser that only
reaches dead cult pages), Snake and Tentacle.

The palette is strictly black and white, the logo is 10x10 pixel art, and
nothing is written to disk.

## The New Gods

Click **The New Gods** on the exposed desktop. Each click sheds particles;
15 clicks within any rolling 10 seconds rupture the title and summon Cthulhu.
The cinematic holds in silence visually, then **THE GODS WILL KNEEL.** slams
into view. A CC0 instrumental hymn fades in over eight seconds while its pitch
rises from 0.22x to 1.05x over sixteen seconds, with distortion and echoes.
CC0 clicks, swishes, metal hits, parries, horror ambience and a deep bell punctuate
the sequence. Music and effects both obey `M` mute.

The native software 3D arena contains three successive sword duels: Ares,
Athena and Zeus. Read their wind-ups, face incoming blows to guard, time a fresh
guard to parry, and punish recovery. Zeus marks the ground before lightning.
The giant witness watches from the north, his pupils following your position.
Victory grants entry to **The Unwritten Tower**, an original four-floor story
chapter: recruit fellow climbers, remember the seal order, defeat the Hollow
Warden and choose the cost of ascent. Companions are local story characters,
not online players. Both endings conclude this chapter; there is no server,
account, persistence or runtime download.

| Control | Action |
| --- | --- |
| W/S, up/down | Move forward/backward |
| A/D | Strafe |
| Left/right arrows | Turn |
| J, Space, left click | Sword attack |
| K, right mouse held | Guard; fresh guard can parry |
| Shift | Dodge backward; costs resolve |
| E, Enter | Speak, invoke a seal, open a gate |
| 1/2 | Dialogue and story choices |
| R | Retry a defeated encounter |
| Esc | Pause; Enter/Esc resumes, Q returns to desktop |
| M | Mute all music and effects |

Offscreen captures: `--screen gods-ritual`, `gods-arena`, `gods-tower`.
`--gods-view 1..10` captures rupture, Athena, Zeus, admission, memory trial,
Warden, final choice, ending, defeat and dialogue respectively. `--time` freezes
the animation in milliseconds; the reveal caption impacts at 3600 ms.
`--headless --check-gods` checks the actual game logic and desktop input routing.
Research and original design decisions: [docs/NEW_GODS.md](docs/NEW_GODS.md).
Audio provenance: [assets/audio/CREDITS.txt](assets/audio/CREDITS.txt).

## Screenshots

The desktop, captured directly from the app at 1080 x 740 with no apps open.
The app captures show each program inside its own window:

![Anaise desktop with ring wallpaper, app icons and no open apps](docs/screenshots/menu.png)

| Startup seraphim | Bethelament |
| --- | --- |
| ![Seraphim startup screen and Anaise title](docs/screenshots/splash.png) | ![Bethelament shelves and glowing pilgrim](docs/screenshots/library.png) |
| **Painter** | **Veil** |
| ![Painter monochrome canvas and brush controls](docs/screenshots/paint.png) | ![Veil browser opening Welcome to the Game](docs/screenshots/browser.png) |

These are offscreen captures at virtual time `4200` ms. To regenerate them,
render into `build/readme`, then re-encode the PNGs losslessly into
`docs/screenshots`; the renderer writes uncompressed files, so the checked-in
copies are re-encoded only to keep the repository small:

```bat
powershell -ExecutionPolicy Bypass -File tools\headless.ps1 -Screen all -OutDir build\readme
```

## Text art

The bookshelf logo and the pilgrim's standing sprite use these exact ASCII
rows in `src/main.cpp`: `#` is a lit pixel and `.` is empty space.

Bookshelf:

```text
..........
.##..##..#
.##..##..#
.##..##..#
.##..##..#
.##..##..#
.##..##..#
##########
..........
..........
```

Pilgrim:

```text
.....####.....
...########...
..##########..
.############.
.############.
.####.##.####.
.############.
.############.
..##########..
....######....
...########...
##############
#.##########.#
#.##########.#
#.##########.#
..##########..
...###..###...
...##....##...
```

<details>
<summary>Seraphim source art (Unicode Braille)</summary>

The original text from `src/angel_art.h`; the splash turns each raised
Braille dot into a point of light.

```text
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⢾⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀⠀⠀⠀⠀⠿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠇⠀⠀⠀⠀⠀⠀⠀⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡠⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠒⠤⠄⠀⡀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠀⠀⠀⠈⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠠⠤⠅⠡⡴⡁⣥⠀⠀⢈⠀⠀⠀⠀⠀⠀⠀⠈⠢⠀⠀⠀⡄⠀⠀⠈⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠊⠀⠂⠀⠀⣼⣿⣿⣿⣿⡓⠰⠄⠠⠄⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⣶⠾⣭⣥⣿⣿⣷⠀⠀⠁⢄⢀⠀⠀⠀⠀⠀⠀⢱⠀⠀⢣⠀⠀⣒⠁⠀⢀⠀⠀⠀⡰⠁⠀⠀⢀⠠⠁⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⣰⢛⣿⣿⣿⣿⣿⣔⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠄⣠⣶⢮⢴⣾⣿⣿⣿⣿⣿⠢⠀⠀⠈⠢⠀⠀⠀⢤⠀⠘⠠⢇⠀⠀⠀⠀⠂⠀⠀⠠⠀⠆⣖⡁⠀⢀⠔⠉⠀⠀⠀⠀⠂⠁⠀⠀⠀⠀⠀⠀⠀⢀⢣⠾⢫⠹⣿⣿⣿⣿⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣨⣾⡿⡡⢴⣿⣿⣿⣿⡿⣿⣿⡷⠀⠀⠀⠈⢇⠀⠠⠀⠁⠀⠘⠁⠀⠀⠠⠀⢀⠀⠀⠀⠀⠀⠀⠃⠔⠁⠀⠀⢀⠤⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢂⣆⢖⣥⡶⡜⣿⣿⣿⣿⡄⠁⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡠⠄⣺⣿⣯⠚⣤⡟⢙⣿⣿⣰⣼⣿⣿⡇⠀⠀⠀⠀⠀⠀⠿⣰⠀⡀⠁⠀⡀⡨⢠⣄⠀⡰⠒⠂⠀⠀⠀⠀⠀⠀⣢⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⣴⡿⣧⢏⢺⣿⣿⣿⣿⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⢈⣺⣿⣿⣷⣵⠏⣀⢪⣿⣿⢸⢼⢸⣿⡇⠀⠀⠀⠀⠀⠀⠀⠁⠀⠐⣀⡈⡨⢘⣛⣤⡤⠃⠲⡥⠆⠀⠀⠀⠠⠞⠓⠀⠀⠀⠀⣀⠀⠄⠀⠀⠀⠀⠀⠀⠈⣼⣿⡽⣫⢯⣿⣿⣿⡿⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡠⢔⠄⠈⣠⣿⣿⣿⣿⣽⠋⣨⠦⣿⣿⣿⡁⣼⠈⢿⣷⠄⠤⣦⣤⢄⡤⠀⠀⠈⠀⠈⣛⢛⣉⣖⣠⠂⢫⣥⣦⣄⢡⡍⠁⠀⠀⠀⠀⠐⠀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⢈⡄⣿⣽⣫⣿⣿⣿⣿⣇⡝⢿⣿⢤⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡴⠊⡰⠣⣪⣾⣿⠿⢿⣷⢋⡝⢁⣵⣿⡿⣽⡟⢑⣣⢰⣿⣿⡄⠀⠀⠀⠀⠃⠀⠀⠀⠀⡻⣧⠻⣿⣿⣿⣷⣄⠶⠬⣍⠀⠞⢀⠀⠀⠀⢠⠠⡄⠀⠀⢀⣀⠀⠀⠀⠀⠀⠀⣾⣀⣿⢡⡏⣿⣿⣿⣿⡇⢸⣿⣿⡆⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⢟⢽⣵⣿⣿⡟⢸⡿⠃⢚⣴⡞⣿⠏⡇⡿⢃⢀⢲⣾⣿⡿⢃⠀⠀⠀⠀⠀⠀⢈⠁⠘⡙⠰⠾⠛⣉⠁⠭⠉⠉⠹⠀⡶⡀⠘⢀⠀⠀⠈⠂⠀⠁⠀⠀⠀⠀⠀⠒⠀⢠⣾⡏⣼⣿⡸⠀⣿⣿⣿⣿⣿⠸⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣰⣷⣽⣿⣿⣿⠟⢡⣌⣵⣿⣿⣿⣇⡏⡄⣷⠃⠾⢙⡤⡿⣿⡓⠀⠀⠀⠁⠀⠃⠀⠀⢠⠘⠀⢰⣶⣾⣿⠐⣿⣿⣂⠂⢩⣿⠌⠄⠺⠥⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⡞⣿⣿⣷⣦⡛⢛⣿⣿⣿⢦⢻⣿⣿⣧⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⠀⢀⣶⣿⣿⣿⡿⠛⣿⠿⢢⣿⣿⣿⡿⠿⣿⣻⢱⣧⢟⡀⢁⡬⢁⣃⡜⠁⠪⠖⠑⣀⠀⠀⠀⠀⠎⠃⡄⠈⣿⣿⣏⣈⣼⣷⣿⣆⢈⠿⡸⠌⢠⡉⠀⠀⠋⢦⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⢿⣿⣿⣿⣿⡏⢻⣿⣿⢸⡞⣿⣿⣿⡆⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠘⠀⣰⣿⣿⣿⠟⢁⣤⡾⢃⣴⣶⠋⣿⡟⡀⣸⢁⣣⢾⡏⣲⢨⣼⡿⠟⠉⡊⠁⢢⠦⠀⠀⢀⠀⠀⠀⠐⣀⣶⡀⠘⢟⡥⣤⣝⣿⣿⠃⠀⣿⡖⠇⠁⢠⠁⠀⠀⠀⠀⠀⠀⢀⠀⠀⢢⢺⣿⣫⣿⢸⠃⣿⠃⣿⡇⣼⠏⣿⡸⣿⢸⣿⣿⣷⡀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠔⠁⢠⣾⣿⣿⡿⡣⣨⡾⡋⡡⢫⢎⡏⢶⣿⡽⡱⣣⣿⡟⣱⠀⣿⢸⡇⡇⣬⡖⡅⠐⠋⠀⠂⠆⠠⠁⠀⠀⠂⠡⠋⢧⠀⠈⠛⠛⠉⠉⠁⠀⠤⡇⣻⡜⢁⠹⡉⠀⠠⠀⠀⠀⠀⠀⡀⢡⣶⡟⡡⢹⠇⠞⠈⡿⠀⠛⠁⠘⠠⢻⡇⣿⡏⣿⣿⢩⣷⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⣿⣿⠋⢠⣾⢟⠅⣞⣱⡇⣼⠇⡾⢈⣭⠶⣿⣿⢸⡏⡖⣿⠟⣥⡇⡿⣱⠓⠃⢉⠀⠐⠚⢄⡀⠀⠀⠀⠀⢁⡀⠀⠀⠀⢴⢀⣤⠀⢐⣔⡝⠘⠅⠂⠄⣡⣥⡄⠐⢆⠀⠠⢊⠀⠋⡜⣱⡫⠛⣂⣰⣦⡥⠠⣾⡩⢰⠀⣸⣧⢻⡗⣿⣿⡎⣿⣇⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⠀⢤⣾⣿⣿⠟⢁⣵⣿⠃⣪⣾⠛⡿⢠⡿⡸⣑⣿⠃⣣⡿⣣⣿⠘⣾⠟⣾⣿⢸⠱⢁⡕⢃⠂⡑⠃⠀⠈⠈⠓⢀⠠⠞⢈⡘⠠⠀⠀⣚⡎⣿⡳⣸⡜⣎⣠⢃⠂⢠⠑⣹⣿⢈⢀⡥⢞⠁⢋⠚⠛⡌⣱⡞⣿⣿⣿⠇⣀⣷⣇⡇⠀⢨⡄⢼⡇⣿⣿⣇⢿⣿⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠋⣴⣿⣿⠟⢅⣴⣿⡟⣡⣾⡿⠃⡪⡐⣼⠰⣶⡷⣶⢨⢹⡿⣸⣿⣾⢇⡇⣿⡟⡀⢞⢈⣶⣃⡾⢡⡄⠀⠂⠀⠀⡤⠺⠾⣿⣷⠾⣯⣵⣾⡿⠜⠷⢎⢫⡏⠎⠀⣺⣆⠲⠄⠂⣉⡀⠰⠿⢗⡳⠶⠔⠾⠿⣧⣿⣿⠏⢰⣸⣼⢙⠀⠐⣾⠂⣼⢳⣿⣿⣿⣻⣿⡇⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⢃⣾⣿⡟⠋⣠⡾⣉⡍⣼⣿⣯⢠⢆⡱⣱⡿⣱⡱⣸⢃⣿⠬⢡⣿⣿⡿⢘⣼⡿⢐⡌⢔⡟⣼⣏⣴⡟⠀⡀⠈⢀⠠⠹⣸⣧⡿⢿⡇⢠⣙⠻⣿⣷⢸⡀⠾⠷⠘⢃⢠⣍⠭⣡⡬⠉⣁⠀⠀⠀⠨⡘⢸⣷⣯⣸⠻⣿⢀⡟⢏⣇⡇⠈⣠⣿⠀⣿⣿⣿⣿⣿⣿⢋⢱⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡠⣰⣿⣿⠿⠀⣾⢏⣾⢏⣾⣿⣿⡏⢸⡄⣱⣿⢳⡵⣱⢏⣾⢡⠂⣽⣿⡿⢡⢸⡟⣠⠟⢨⣾⢣⡎⣿⠛⡐⡼⠁⢠⣾⡟⢣⣟⢙⣷⢠⡃⡌⢻⣧⣿⣿⡞⠡⠼⠃⢃⣼⣯⣦⠜⢈⠔⠼⠃⡀⠸⣕⣀⠈⠸⣿⡇⠛⡄⠁⢹⡇⣼⣸⠇⠄⣿⡇⠁⣿⣾⣿⣿⣧⢻⢸⢸⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣎⣽⠿⣿⡏⣼⡿⣱⠿⣳⣿⣿⡿⠟⢵⣬⢰⣿⡇⠟⣴⢣⠻⢏⣴⣧⡛⣩⣶⣫⡜⠘⡱⢨⣿⢏⡦⣇⡾⣀⡜⢠⠀⠸⣿⠿⡚⠃⢾⣿⡆⣧⢻⣎⠙⢻⣽⣿⣿⣿⠈⣰⣞⢏⡡⢞⣳⡛⠀⠀⣴⣶⣿⡿⡄⠀⢻⡧⠃⣁⡄⣿⡇⡛⣿⠠⢸⣿⠀⠠⣽⢺⣿⣿⡇⣿⠸⢸⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⢡⣾⡟⢸⡿⣸⢟⡼⠃⣼⣿⣿⡿⠁⣿⣷⣭⡬⢭⣶⣿⣶⣿⣿⡿⢋⢿⣿⣿⡟⡤⠀⣾⢃⣚⡡⣿⣷⠉⡚⠀⠀⡄⡄⢐⡉⠑⠀⠹⢹⣿⡿⣨⡀⢻⣦⡁⢝⠻⠿⠁⣸⠹⠃⠈⠁⠦⠭⡄⠀⢰⣿⣿⣿⠂⢰⣆⠘⢁⢀⠈⢁⢹⠃⣸⡿⠀⣿⣿⠘⢸⣿⢹⣿⣿⣯⣿⠁⣾⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⠁⢼⣿⠀⡟⣱⠫⡾⠁⣼⣿⣿⡟⠀⣼⣿⣿⡏⠖⣾⣿⡟⢡⣸⡟⣱⢸⣹⢻⣿⢃⠀⢰⢃⣾⡟⢹⡿⡇⣴⠱⠀⡘⡥⣾⢯⣻⠚⠆⠲⢸⣿⣷⠰⣧⣠⡹⣝⠢⣶⠖⢋⡁⢆⠆⢾⡇⠋⡉⠬⢠⡿⠋⢀⡄⡀⢀⣿⡄⠏⠀⠀⠠⣿⣾⡟⠇⢱⣿⠃⢸⠨⢱⣿⣿⣿⣧⣿⠠⢹⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⠟⣠⠸⢃⣦⣾⠁⠀⢅⣾⣿⣿⡟⣼⡂⢿⣿⡿⢡⡇⣿⣿⠁⣣⢏⣴⡟⣰⡆⣸⠁⠀⠀⢈⡞⢛⡀⠌⠁⢅⡧⡇⠢⡘⣷⣎⠓⢽⣷⠑⠜⠘⣿⣿⡄⠹⠇⠁⣴⣶⣎⢬⠣⣍⠼⢰⡀⢥⣶⡄⠓⠃⢀⣴⢻⠐⡔⠃⢀⠃⠄⠀⠀⡀⠽⡟⢐⠠⢸⡿⠐⡚⢰⣾⣿⣿⣿⢻⣿⡄⢸⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡏⣰⣿⣶⣿⣿⢁⠿⢏⣾⣿⣿⠟⢸⣿⣧⢌⣩⣴⢸⣿⣌⡓⢀⣵⣿⠏⠄⠛⠁⠋⡄⢠⡾⠿⠁⢋⠃⣆⠂⣼⢙⠐⠟⠛⣿⣿⣷⣦⠙⣦⢸⡆⣿⣿⣷⣠⣄⠠⡀⠈⠉⠦⠇⢈⡉⠋⠣⠀⠏⢈⣷⣿⣾⡶⢖⡌⠀⡴⠈⡐⣰⡖⠀⠃⢀⢁⡛⡀⢘⠇⣿⡇⣿⣿⣿⣿⣿⣹⣿⣇⠸⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⡀⡟⢰⣿⣿⣿⣿⠃⣼⠃⣼⢏⢸⣿⢃⣿⣿⢋⣾⣿⣿⠇⣭⡭⢶⣷⣭⣵⡣⠄⢠⡾⡇⢧⡄⠁⠀⠀⠋⠀⠛⠐⣨⡀⠠⣰⣔⠙⢿⢿⡟⠠⣋⢸⡧⠙⠒⣨⡝⢩⣦⠇⠁⣤⠘⢤⡌⠀⢰⠠⠐⡄⠉⠙⠫⡁⠠⠤⠂⢤⠀⣠⢃⢻⠁⠀⠀⢀⡿⡄⠀⣸⢰⣿⠃⣿⣿⣿⣿⣿⣽⣿⣿⠀⡇⠀
⠀⠀⠀⠀⠀⠀⠀⡆⠈⠀⣿⣿⣿⣿⡏⠀⢇⡿⡇⣸⢸⡇⣸⣿⠇⢸⣿⣿⡇⣾⣿⣇⣾⣿⢿⣿⠇⢀⡎⠀⠁⣾⡇⢀⠀⠀⠈⠀⠀⣀⣾⡇⣸⣿⣿⣿⣿⡆⠀⣷⢱⠉⣿⠆⠀⠀⢹⣇⡆⢁⣾⠸⡀⠀⠉⠇⠁⡇⡆⣶⡿⡇⢀⣷⡶⠿⣆⠹⡆⠁⢸⡀⠀⠁⡇⢹⣷⠇⣶⡿⣸⣿⠀⣿⣿⣿⣿⣿⣾⣿⠀⡆⠇⠀
⠀⠀⠀⠀⠀⠀⠰⠀⠀⣸⣿⣿⣿⠋⠀⡸⡸⠝⢰⡇⠟⢠⣿⡟⡰⣿⣿⢈⣴⢫⡻⣿⢟⣵⣷⠉⠀⠈⠀⠀⢠⡟⠒⠋⠀⠀⠀⠀⣰⠿⣀⣀⡀⠹⣿⣿⠟⠠⡀⠻⠰⠺⡏⠜⡢⡙⠖⠆⣶⡮⠈⠃⠩⣒⡟⡴⠀⢇⢏⠯⣓⡃⠼⠃⣐⢣⡠⠴⠂⡀⠀⠷⡀⢲⠁⣼⡾⢠⡟⡀⢃⣿⡄⣿⣿⣿⣿⣿⣿⣻⠀⡡⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⡱⣿⣿⡿⠃⣾⣀⠣⣼⣿⡜⢟⡰⣜⠿⢃⣅⣛⣵⡿⢱⡎⡏⣏⡾⠛⢋⠀⠀⠀⠀⠁⡎⠀⠀⠐⠐⡉⠄⠐⢉⢺⣟⡛⢿⣦⣹⡏⢀⠴⡄⢡⠐⠀⢿⣤⠿⠘⠠⢜⠷⠉⠺⠖⠀⠉⠙⣄⣀⠾⣅⡍⠒⠠⠤⠌⠹⣷⣦⠃⠡⠁⠀⠀⢒⡘⠀⢹⠧⣾⣿⠀⣸⣏⢸⣿⣿⣿⣿⣿⣽⣿⠀⢧⠀⠀
⠀⠀⠀⠀⠀⠀⠀⣼⢳⣿⡿⣡⡺⡋⡋⡆⣾⡿⠙⣿⢏⣶⡊⢹⡿⣁⣭⣶⢸⣳⡇⠋⠀⠀⢈⢰⠃⠀⠀⠘⠁⠀⠀⠀⠀⠀⠀⢄⣤⣟⣿⣏⠀⠘⢓⡵⢃⡆⠇⢿⡄⢮⠤⠃⠔⠋⣠⡄⠄⠀⠀⢠⣁⣀⣉⡀⣤⠆⠐⠘⠋⠱⣶⡉⠓⠈⣿⡄⠀⠀⠀⡈⠠⡇⠀⢀⠀⠻⢠⠚⢻⣽⢺⣿⣿⣿⣿⣿⣿⡇⠆⠸⠀⡄
⠀⠀⠀⠀⠀⠠⢰⡏⣿⣿⣇⣿⡟⡼⣸⠅⣿⠇⢲⢏⣾⣿⡟⣬⡎⡇⣿⡟⣾⢇⠾⠿⠀⠀⠀⠏⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⡗⡀⢙⣦⠈⡄⠀⠸⣿⣦⣠⢶⠮⡁⢠⣴⣦⠰⣇⠀⠀⠚⠉⢛⣠⡌⢉⣉⡉⢒⣉⣽⡀⠀⢸⠂⠀⠀⠰⠁⡄⠀⠀⠀⠆⡆⢀⠷⢵⢸⣹⣿⣿⣿⣿⣿⣿⠸⠁⠂⠀⡇
⠀⠀⠀⠀⠀⠄⣿⣸⣿⡿⠩⡛⣼⢣⣿⣇⠻⢠⣾⣿⠏⣾⢣⣿⢸⢵⡿⡸⢣⠂⠀⠀⠁⠀⠀⠀⠃⠀⠀⠀⠀⠀⠀⠀⣀⣼⣿⣿⣿⣿⠟⢩⡴⠒⠀⣿⡆⢱⠀⣿⣿⣿⠋⣪⡞⣵⣿⣿⡟⣃⠁⠀⠥⣔⢒⣪⠙⣇⠺⡲⠸⢤⠏⠚⡇⠀⢨⠀⠀⠀⠙⠐⠃⠀⠠⠀⠸⠐⡼⡀⡿⣼⣹⣽⣿⣿⣿⣿⡿⠀⠄⡁⠀⡁
⠀⠀⠀⠀⠐⠀⢁⡏⡿⣱⢧⣰⡿⣼⣿⣿⣷⡘⡿⠁⣰⡏⢟⣅⡟⢬⠔⠀⣋⠚⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⠟⣉⠀⠈⠀⠀⢰⣚⢤⡯⢆⣿⡿⠃⠀⣀⢾⣿⣿⣿⣿⠋⣰⠋⡴⣇⡌⠉⠁⠘⢆⢝⣃⠈⠓⢹⣧⠀⠀⠀⠀⠀⠀⠀⠂⠁⠐⠆⠀⣱⢱⡇⠄⣿⢼⣿⣿⣿⣿⠟⡧⢈⠐⡀⠀⡄
⠀⠀⠀⠀⠀⠰⣼⣈⣵⣿⢸⣿⢣⣿⣿⣿⣿⣿⠗⠀⣴⢹⣻⢉⣾⡇⠀⣾⡇⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⣿⣿⣿⡿⠫⠈⠀⢋⣄⠀⠀⠀⡯⢧⢲⠀⣴⣾⠏⣰⠏⣼⠟⡿⢟⠕⢀⣜⡒⠒⠉⡍⠐⠒⢦⠈⢒⠍⠠⠀⠃⢈⡁⠀⠀⠀⠀⠀⠀⠀⠀⣼⠁⠁⡀⣾⠙⣠⠈⢸⣿⣿⣿⣿⣾⡗⢀⠂⠄⠀⡇
⠀⠀⠀⠀⠀⠀⣿⣿⣿⡿⠀⣿⣼⣿⣿⣿⡿⢡⡆⢸⣿⢸⠁⢰⣿⠀⠁⡿⠆⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⣠⣿⣿⣿⡿⠋⠀⠀⠀⠰⢸⣿⡀⠀⠀⠐⡋⣡⣾⣿⣜⢓⢋⢚⡋⡖⠵⠃⢀⣿⣿⡇⠀⠉⠆⠢⢤⠎⢀⡼⢺⡟⠤⣈⣿⣿⠀⠀⡐⠀⠀⠀⠀⢀⡏⠀⢀⢀⢁⠄⠨⠀⣼⣿⣿⣿⡏⣿⡇⡀⠈⠂⠀⡇
⠀⠀⠀⠀⠀⢸⣿⣿⣿⡇⢾⡇⣿⠟⠉⣿⠇⠘⠠⢼⡟⢎⡀⡸⠇⠀⡐⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⡏⠀⠀⠀⠀⠀⠀⠀⢻⡇⠀⠀⠀⣴⣿⣿⢻⣿⠸⢸⠨⠆⡠⣢⠮⠘⠋⣿⡗⣀⠈⠌⠙⠒⠒⠀⠀⡨⣀⠀⠀⢻⡇⠀⡆⡅⠀⠀⠀⠀⠜⡡⠀⠀⠱⣸⠀⠀⣾⣿⣿⣿⣿⡇⣿⠃⣀⠠⠁⠀⡅
⠀⠀⠀⠀⠰⢺⣿⣿⣿⢰⢼⢹⡏⠀⠀⠟⠀⠀⡎⠘⡇⣬⢰⠁⠀⠐⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣿⣿⣹⠋⠀⠀⠀⠀⠀⠀⠀⠀⠈⠡⠇⠀⠸⠿⠛⠛⢋⣿⣦⣽⣾⣜⠅⠙⡱⣫⠆⣿⡧⠟⠀⠀⢈⠀⣐⣀⠰⣀⠴⠿⢃⠘⠇⠀⠇⡆⠀⠀⠀⠀⠋⠁⠀⡃⠅⠃⢀⠘⠋⠙⡜⣿⣿⢧⡟⠀⠀⠀⠀⠀⡆
⠀⠀⠀⠀⡞⢸⣿⣿⣿⠈⡎⣿⠃⡀⠀⣠⣴⡆⠀⠈⢡⡏⡋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣼⣿⣿⣿⢨⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠡⡀⢰⠈⢁⣵⣿⣿⣿⣿⠿⢋⣥⣬⣾⡧⣤⣿⡗⠀⠀⢰⠇⠠⣟⢈⠆⠘⠆⣉⢀⢲⡇⠨⠁⠀⡅⠀⠀⠀⠀⠀⠰⢀⠀⣸⣹⠇⡇⢐⠇⣸⣿⢸⢃⠀⡄⠀⠀⠀⠁
⠀⠀⠀⢰⡧⣾⣿⣿⡏⡀⠛⢉⡾⠈⣸⡏⠛⠃⠀⠀⣼⢠⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⣿⣿⣿⡿⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⠀⣼⣿⣿⡿⠟⡉⢀⣴⣿⠟⠉⡀⣀⢀⣶⣷⠀⡀⡈⠆⣉⣤⣤⠂⣘⠶⣿⣿⠰⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⠀⠀⢆⠏⣰⡇⡰⣠⣿⡿⡹⠆⠸⠀⠀⠀⠀⠀
⠀⠀⠀⠘⢰⢻⣿⣿⡏⢸⣿⠀⠆⠀⠛⣿⠆⢠⠀⠀⠈⠂⡞⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⣿⣿⡿⣿⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡿⠋⠁⠠⣀⣴⠿⠋⣡⠎⠀⣄⠿⢸⣿⣿⡇⢠⠣⡀⠉⠄⢢⠅⠀⠀⣂⣽⡧⢹⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠁⢁⣰⢀⢻⣇⢑⣿⣿⢇⣇⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠘⣸⣿⣿⢇⢸⣿⡄⠀⡇⠀⠙⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢾⣿⣿⠟⠀⠑⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡛⢉⣤⣶⠿⠋⠁⠀⢠⣏⡀⡞⠈⠈⠁⢻⣿⡇⠈⠃⠀⣙⡁⠎⠡⠀⠀⣿⢸⠃⠀⢀⠀⠀⠐⠀⠀⢀⠀⠀⠌⠀⠄⢫⠏⡁⢎⣽⣿⣿⢸⣿⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⣬⢼⣿⣿⢸⢸⣿⣿⠀⠁⡇⠀⡀⢸⠀⠠⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⣿⠁⠀⢀⣀⢻⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠵⠟⢉⠠⢒⣤⡞⢁⣬⣾⠗⠀⣤⡷⠶⢸⣿⡃⢸⡄⠀⡜⢧⠰⢲⢸⠀⡟⠡⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠒⢢⣴⢃⣼⣿⣿⣇⣿⠊⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢠⡻⣼⣿⣿⠈⣾⠻⠏⡠⠰⠃⠹⢣⡌⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠀⣴⠋⣘⡈⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣤⣵⣾⣿⣫⡾⢟⠛⣡⣴⠿⠫⠄⠸⡌⢿⣿⠈⣷⡇⠃⠘⠀⠘⠀⠀⠡⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⢔⡿⡃⣾⣿⣿⡿⣸⠃⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢸⠇⣿⣿⣟⡰⣷⠘⠀⠁⠀⠀⠊⠂⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠀⣩⠈⠈⢃⠀⡤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⣿⣿⠟⡋⠡⣰⠟⡋⢁⢰⠆⠀⡆⠱⢨⣿⡅⣟⠇⢈⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢐⢠⡿⡱⣹⣿⣿⢋⣵⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢸⠇⣿⣿⡏⢧⠿⠅⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡇⠹⣄⠀⣸⠂⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⠿⠟⣋⢠⣢⠖⡊⢔⣠⢾⡏⠏⠀⣀⣀⠄⠀⢿⣷⠨⠃⠈⠀⠀⢃⠀⡈⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⠀⠀⢀⠘⠄⢰⣿⣿⢣⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣼⡞⣿⣿⡇⠸⠀⠀⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢁⣀⠈⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⠐⢪⡴⠋⠔⢉⣴⣿⠃⠈⡀⣠⡞⠹⣿⡀⠀⢺⡇⠀⢀⠀⠀⢰⠈⠀⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠁⠀⡆⠈⢀⣼⣿⢏⣾⡿⠀⠀⠰⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠂⠀⣿⢿⣿⣿⡇⢀⠀⠀⠘⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣶⣿⣽⣰⣾⣿⡿⠋⠀⢁⠂⡟⠀⢸⡽⠅⠃⠨⣷⠈⢸⠀⠀⣼⠄⠀⠀⠀⠀⠀⠀⠀⠀⠂⠀⠀⠀⠀⠀⣼⠃⢠⣾⣿⣏⣼⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣿⣸⣿⡣⠇⡘⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠛⠛⠿⢿⣿⠿⠁⡁⢸⣷⡿⢠⡗⠂⡇⢢⢠⢰⠄⢺⣦⠀⢌⢃⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⠃⡴⣼⣿⡟⣼⠋⠀⠀⠀⢀⠂⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣿⣿⣽⠃⠄⡏⢀⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⢠⢰⣔⠈⠀⡁⢾⢟⣵⢺⡇⠁⣇⢸⡏⠠⡄⠈⣶⡆⣄⣾⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⡐⣴⣿⣯⠚⠁⠌⠀⠀⠀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢱⣿⣸⠀⠁⡇⠘⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠿⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠀⠈⠿⡘⠃⢁⣴⣿⣿⡌⣿⡆⢹⠀⣗⡂⣇⡁⣄⢨⠀⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣿⣿⠏⠀⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢸⣿⢸⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠂⠠⢀⠱⣴⠛⠿⣿⣿⣿⣧⣄⣾⠃⡻⣧⢿⡿⠀⡒⡄⡄⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠂⠐⠀⡀⣼⣿⡏⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⡄⠸⡏⡇⣦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠘⠌⢿⣠⣾⣿⣿⣿⣿⣿⣿⣿⣆⢿⡸⣧⠀⠇⢃⠀⠘⡀⡂⠀⠀⠀⠀⠀⠀⠀⠀⠁⢤⡎⠼⣾⣿⡿⢁⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⢱⠀⡇⢰⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡄⠀⠈⢻⣿⡿⣿⣿⣿⡇⣿⣿⣿⣼⡇⣿⣇⠚⡈⡜⣦⠡⡅⠀⠀⠀⠀⠀⠀⠀⡠⢠⡆⢠⠞⣛⠿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⡃⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⠸⣟⣽⣿⣿⣿⡇⢿⣿⣿⡏⣧⢿⢷⠠⢙⢧⢿⠰⣡⠀⠀⠀⠀⠀⠀⠀⣠⢇⡘⢋⣴⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⢀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⣸⠿⣹⣿⣿⠃⢻⣿⣿⣷⣿⡞⠾⢎⠼⣌⡜⡜⠦⢃⠀⠀⠀⠠⠁⠖⢩⡎⡵⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
```

</details>

## Features

- Pure black (`#000000`) and white (`#FFFFFF`) throughout
- Seraphim startup screen drawn as a field of dots, revealed outward from the
  centre, then the title sputters on like a failing tube
- An OS-style desktop with a slim monitor outline and clear monochrome app icons
- Independent draggable app windows with minimize, maximize/restore and close,
  plus a taskbar tab for each open app. The desktop starts with no apps open
- A CC0 ambient score: an intro choir under the seraphim, a dark shrine loop on
  the menu and a cathedral loop in the rooms. Click the volume icon, or press
  `M` anywhere, to mute
- A slowly turning orthogonal ring pattern glows behind every screen except
  the startup: rings of circles that cross each other at right angles, joined
  by straight chords and dithered to pure black and white. On the menu it is
  the wallpaper behind the icons.
- **Bethelament**: a quiet room of shelves and bookcases, with a small hooded
  pilgrim you walk around it using WASD or the arrow keys
- **After School**: finish the four 2D puzzle rooms and follow the crash/search
  transition into a first-person school dating sim. Explore a 31 x 25 campus
  with classrooms, lockers, windows, and four club girls with different interests.
  Dialogue choices build individual hearts and a shared rizz score; earn at
  least three hearts and invite someone to the festival for her ending
- **Painter**: a 32x32 monochrome canvas with brush sizes (1-3 or the mouse
  wheel), erasing, undo and redo, and clear
- **Veil**: a dial-up browser of fifteen dead pages, with tabs, back, forward,
  reload and a link to follow. It never touches the network
- 10x10 pixel-art bookshelf logo, used both in the window and as the app icon
- Titles set in Squealer, a CC0 blackletter typeface embedded in the executable,
  and the desktop dedication set in UnifrakturMaguntia, a SIL OFL blackletter
  face embedded beside it
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

## Background rings

Every screen after the startup hangs a large ring pattern behind it, drawn in
the spirit of the orthogonal ring patterns in the plane: a ring of circles that
cross their neighbours at right angles, and each ring inward crossing the ring
before it at right angles as well. Straight chords join the circle centres, so
the figure carries both the rings and the web of lines through them.

The pattern turns very slowly. Alternate rings counter-rotate, the whole figure
breathes and drifts a few pixels, and an arm of light travels around it, so it
never repeats exactly. The drawing is not made of lines at all: each circle and
chord adds light to a field, the field is ordered-dithered with a Bayer matrix,
and only pure black and white pixels are written to the window. The glow is
therefore made of white dust rather than greys, which keeps the palette intact.

The pattern fades out under the page header and footer of Bethelament and the
Painter canvas. On the menu it runs edge to edge inside the monitor glass, as
the desktop wallpaper, combed with CRT scanlines and covered only by the solid
black shell.

## Main menu

The desktop starts with no apps open. Its larger dithered case pattern reaches
all four edges and stays anchored when the native window is resized. The ring
wallpaper also keeps its position and scale as resizing reveals more of it.

Click a desktop icon, or use Up/Down and Enter (or Space), to launch an app.
Drag an icon to arrange it anywhere on the desktop; dropping it does not launch
the app. Positions last for the current session and stay inside the desktop on
resize. Each app opens inside its own window. Click a window to bring it
forward, drag its title bar to move it, use the three title buttons to
minimize, maximize/restore or close it, and click its taskbar tab to switch to
it or restore it. Clicking the active tab minimizes that app. Minimized apps
keep their canvas, room and browser state. Closing an app removes its tab;
closing an in-game window leaves the machine running. `Esc` minimizes the
focused app, and clicking the desktop returns focus to it. The Quit icon or
the native close button exits.

The desktop has no top menu bar or border lettering, apart from the dedication:
`The New Gods` sits at the top right in a blackletter face, and every wallpaper
ring and scanline that passes under it comes out inverted, so the type is cut
out of the moving background rather than drawn on top of it. The small logo
button opens the app menu, which lists the three apps and Quit. The clock and
clickable volume icon sit at the bottom right; click the speaker or press `M`
to mute. Painter has a staggered, etched Hebrew wallpaper spelling "dissolve
and solidify" around its black canvas. Keyboard instructions are kept out of
the in-game interface.

## Music

Three public-domain tracks are embedded in the executable as mono 22.05 kHz
WAVs, so there is no soundtrack folder and nothing to download. The intro plays
under the seraphim and hands over to the menu loop; Bethelament and the Painter
share the room loop. Changes crossfade over about half a second, and the loops
were seam-edited before conversion so they do not click.

| Track | Author | Source |
| --- | --- | --- |
| Intro: *The Fade* | Breadman | <https://opengameart.org/content/the-fade> |
| Menu: *Dark Shrine Loop* | qubodup (remix of yd) | <https://opengameart.org/content/dark-shrine-loop> |
| Rooms: *Cathedral in the Forest* | congusbongus | <https://opengameart.org/content/cathedral-in-the-forest-ambient-loop> |

All three are CC0 1.0 Universal (public domain dedication), so no credit is
required; the table and `assets/audio/CREDITS.txt` record the provenance
anyway. Audio is optional: if no output device can be opened the app runs
silently. Click the volume icon at the lower right, or press `M`, to mute or
unmute.

## Bethelament

Bethelament is a 13 x 9 tile room drawn in the same dots and bars as the rest of
the app. A small hooded pilgrim walks it the way an RPG Maker character moves:
one tile per step, four facings, and a three-frame walk cycle (left step,
standing, right step) that falls back to the standing frame when they stop.
Holding two directions walks in the one pressed last, and the walls and
bookcases block the way. `Esc` minimizes the room window.

While a step is in flight the pilgrim turns into light: the silhouette leaves a
short afterimage along the axis it is walking on, never across it, and sits in a
halo that fades out through an ordered dither. The smear swells mid-step and is
gone by the time the next square is reached; standing still, only a quiet glow
remains. Everything stays black and white, the same way the ring pattern does.

The space around the pilgrim bends as well. The room is drawn to an offscreen
surface, then sampled through a small barrel lens that magnifies and turns the
floor around the walker and combs a slow ripple through it, and only then is the
result copied back and the pilgrim drawn on top. The lens strengthens mid-step
and settles to a faint shimmer at rest. Sampling keeps a pixel-sized footprint, so
the room's one-pixel rug and shelf lines bend instead of breaking into dashes,
and the output is still nothing but black and white.

## After School (3D)

The school follows Bethelament's key, lever, and ordered-plate puzzles and the
final breach. Enter or click through the crash/search sequence, or let it play
automatically. The college has eight classrooms and four club rooms connected by
wide halls. Its map shows the campus and your facing direction.

Meet Hana in the northwest art studio, Rei in the northeast gym, Yumi in the
southwest library, and Mika in the southeast music room. Each has three
conversations and a distinct sprite, personality, and festival ending. Thoughtful
answers earn two hearts and ten rizz; ordinary answers earn one heart and three
rizz; rude answers lose one heart and five rizz. Hearts stay between zero and
five, and rizz stays between zero and 100. After three conversations you can
invite her to the festival with three hearts, talk more, or leave.

Use `W/S` or Up/Down to walk, `A/D` or Left/Right to turn, `E` to talk, and
`1`-`3` to choose an answer. `Backspace` leaves a conversation. `R` restarts the
school; `Esc` minimizes it and preserves progress. On a festival ending,
`Enter` returns to the 2D rooms. Everything lasts for the current session.

All four characters are entirely fictional adults aged 25-27, with photorealistic
generated faces in the world and conversation panels. Six expressions cover
neutral, happy, shy, hurt, terrified and dead states. Names appear alone; there
are no visible emotion labels. The former real-person stock-photo cast has been
replaced. [Character designs and generation prompts](assets/school/CREDITS.md)
are included with the color masters in `assets/school/masters`.

A hidden knife can be found in the north classroom. Search near it with `E`,
then use `Q` to equip or put it away. `F` or a left click attacks within reach
and in front of you; walls block hits. Three hits are fatal. Exposing the knife
frightens nearby witnesses; attacks trigger screams, fleeing, face changes,
blood bursts, floor pools and blade stains. Wounded characters run more slowly;
survivors find routes through doorways to the hall exits. Hold `Shift` to sprint.
`R` starts a fresh college day, restoring everyone and clearing the knife/blood.
Blood effects use the existing pure black-and-white palette.

The college plays **Cate** by Cakeflaps / You're Perfect Studio, selected under
its CC0 license. During panic the same tune slows and drifts out of tune. Two
acted female screams by **missozzy** are also CC0. Source pages, download URLs,
processing and resource IDs are recorded in `assets/audio/CREDITS.txt`. All
music and screams are embedded and obey `M` mute; there is no runtime network.

Preview without opening a window:

```powershell
powershell -ExecutionPolicy Bypass -File tools\headless.ps1 -Screen corridor -SchoolView 1
```

`-SchoolView 0` shows exploration, `1` a conversation, `2` a festival
ending, `3` an equipped knife and frightened character, and `4` the aftermath.
The equivalent executable flag is `--school-view 0|1|2|3|4` with
`--headless --screen corridor`. `--check-game` checks the full 2D-to-3D flow,
room reachability, collision, dialogue, hearts, invitations, and restart.
Add `-SchoolGirl 1..4 -SchoolExpression neutral|happy|shy|hurt|afraid|dead` to preview any
girl and expression, or use `--school-girl` and `--school-expression` directly.

## Painter

Painter is a 32 x 32 monochrome canvas. The number keys `1`-`3` and the mouse
wheel choose a square brush from one to three cells. The left button draws and
the right button erases; `Ctrl+Z` undoes, and `Ctrl+Y` or `Ctrl+Shift+Z` redoes.
`C` clears the canvas. An opaque black square keeps the drawing separate from
the dithered, etched Hebrew wallpaper that tiles around it and spells "dissolve
and solidify".

## Veil

Veil is a browser that outlived the network it was built for. Fifteen addresses
are stored in the executable, so every page is local and no socket is ever
opened: a welcome page, a shrine, an order, a guestbook that should not exist,
and the rest of the crawl.

Up to six independent tabs retain their own navigation history and scroll
position. Click `+` or press `Ctrl+T` to open a tab; click its `x` or press
`Ctrl+W` to close it. `Ctrl+Tab` switches tabs. The arrows traverse actual
visited pages (`Left`/`Backspace` and `Right`); following a new link after
going back replaces that tab's forward history.

Click the address field or press `Ctrl+L`, then type a local address or search
the directory by title or host. `Ctrl+A` selects the field, `Ctrl+V` pastes,
`Backspace` edits and `Enter` navigates. Unknown addresses show an empty
search result. The directory lists all fifteen hosts. `Ctrl+H` opens history;
`Ctrl+B` opens bookmarks; `Ctrl+D` or the plate beside the address toggles a
bookmark. Panels support wheel scrolling and clickable records.

The wheel, arrow keys, `Page Up`/`Page Down`, `Space`, `Home` and `End` scroll
the document. `Enter` follows its link; `F5` or `Ctrl+R` reloads. `Esc` cancels
address editing or closes a browser panel, then minimizes the browser.
All tabs, bookmarks and history last only for this session.

Scrolling below a site's original page reveals photographs of
abandoned rooms, impossible occupants and unidentified faces. Slow presence
events interrupt documents, and the portrait shifts with hovered controls. Its
faces use four distinct CC0 sources: rottonara's elderly-man photograph,
an anonymous old-man portrait from PublicDomainPictures, Martin Vorel's
photograph of a wooden puppet, and Hugo van der Goes's portrait from The Met.
A magnifying lens periodically glides down each image, bending the face as
it passes, then rests before repeating. Hosts and archive scenes show different
faces; all animation uses the same deterministic clock as headless captures.
The archive
uses dithered derivatives of Sergej Majboroda's CC0
[Old Hall](https://polyhaven.com/a/old_hall) and
[Abandoned Church](https://polyhaven.com/a/abandoned_church) assets from
Poly Haven. Provenance and license links are in `assets/browser/CREDITS.txt`.
Every page spends a moment loading behind a progress bar
before it paints, and the reload plate repeats that wait. Hovering a control
draws a hand cursor and inverts the plate.

Offscreen previews also accept `--browser-scroll 0..320` and
`--browser-panel directory|history|bookmarks`. `--check-browser` exercises
tab isolation, history branching, address entry, search and bookmarks without
opening a window.

## Build

Requires MinGW-w64 (`g++` and `windres`) on `PATH`.

```bat
build.bat
run.bat
test.bat
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

## Testing

Everything is headless by default. The app can render any screen into a PNG
without creating a window, so builds, tests and captures never flash on screen
or steal focus.

```bat
powershell -ExecutionPolicy Bypass -File tools\test.ps1
```

The test script builds the app, renders every screen offscreen, and checks the
captures: size, palette, determinism, the splash reveal, a frozen Bethelament
step and a Veil page, plus argument handling. It also parses the embedded
audio and exercises the desktop window lifecycle offscreen. It prints one line
per check and exits non-zero if anything fails; add `-NoBuild` to skip the
build.

To just save some screens while working:

```bat
powershell -ExecutionPolicy Bypass -File tools\headless.ps1 -Screen menu
```

Or call the renderer directly. `--screen` accepts `splash`, `menu`, `library`,
`paint`, `browser` or `all`; `--time` is the virtual millisecond at which the frame is
taken, so a fixed time always produces the same image. For a frozen step in
Bethelament, add `--walk up|down|left|right` and optionally `--progress 0..100`
to place the pilgrim partway between two squares. For Veil, use
`--screen browser --page 1..15` to choose a page.

```bat
bin\Anaise.exe --headless --screen all --size 1080x740 --time 4200 --out build\headless
bin\Anaise.exe --headless --screen library --time 4200 --walk right --progress 50 --out build\headless
```

`tools\capture.ps1` still exists for grabbing a running window, but it uses
`PrintWindow` and does not activate or raise the window.

## Layout

```
src/main.cpp       Win32 window, menu, startup art, Bethelament, Painter, Veil
src/audio.h        waveOut streaming for the embedded CC0 tracks
src/ring_pattern.h the orthogonal ring pattern behind the working screens
src/occult_art.h   dot art for the Veil pages
src/angel_art.h    Braille seraphim source art
src/app.rc         icon, embedded typefaces, music, version info
assets/             generated 10x10 pixel-art icon
assets/fonts/       Squealer (CC0 1.0) and UnifrakturMaguntia (SIL OFL 1.1)
assets/audio/       the three CC0 tracks as WAV, with provenance
docs/screenshots/   losslessly compressed captures for this README
tools/              icon generator, headless capture and test helpers
```

## Fonts

The title uses [Squealer](https://www.dafont.com/squealer.font) by Raymond
Larabie (Typodermic Fonts), released under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). The `.otf` is
compiled into `Anaise.exe` as a resource and registered at startup, so nothing
has to be installed. See `assets/fonts/squealer-LICENSE.txt`.

The desktop dedication uses
[UnifrakturMaguntia](https://fonts.google.com/specimen/UnifrakturMaguntia) by
j. 'mach' wust and Peter Wiegel, released under the
[SIL Open Font License 1.1](https://openfontlicense.org/). Its `.ttf` is
embedded the same way. See `assets/fonts/unifrakturmaguntia-OFL.txt`.

## Snake

Open SNAKE on the desktop or Start menu. Enter starts; arrows or WASD steer.
Aim the drill head at the forehead crosshair. Every hit grows the snake, adds
one point and increases speed; walls and your own body jam the drill.
Three cropped historical portraits from The Met's CC0 Open Access collection
cycle as targets. Each hit freezes the board for a 3.6-second zoom-in cinematic: the drill
moves in, a CC0 acted groan plays at contact, the eyes roll up and the mouth
drools. A next-level title reveals the next portrait before play resumes.
Levels increase the speed and keep the growing snake. Credits: `assets/snake/CREDITS.md`.
Space/P pauses the game and cinematic voice, R resets, Enter retries after a loss. Switching away pauses;
Space resumes after returning. Progress lasts for the open app's session.
Offscreen: `--screen snake`, `--snake-view 1` previews a hit reaction;
`--time` sets the sequence age: 100 zooms in, 800 reacts, 3200 reveals the
next person, and 4000 shows the next level. CC0 sound sources and edits are in
`assets/audio/CREDITS.txt`; `tools/prepare_snake_audio.py` rebuilds the clips.
`--headless --check-snake` checks movement, scoring, collisions and shell routing.

## Tentacle

Open TENTACLE on the desktop or Start menu. A great mouth sits at the end of a
walled maze and lasers whatever reaches it. Blobs pour out of the holes on the
left and, from wave 6, out of two flanking holes mid-maze. They share one flow
field towards the mouth and steer smoothly round every corner, so a corridor
fills like a river. Each species is drawn procedurally and behaves differently:

- **Mites** are small arrowheads that dart in bursts and weave.
- **Puddles** pack tightly, and two puddles that touch run together into a bigger, slower one.
- **Splitters** burst into three mites when they die.
- **Shells** are armoured hexagons that take less laser damage and shove the crowd aside.
- **Jellies** trail tentacles and make everything near them faster and self-healing.
- **Brutes** arrive every fifth wave: huge, many-eyed and they bite hard.

Grow defenders with 1-4 (or the bottom bar): the **Biter** (a cheap, steady
short beam), the **Stalk** (an eye whose long beam pierces six in a line), the
**Maw** (a blocker that bites everything around it) and the **Digester** (no
teeth, passive biomass). Drag a box to select, then right-click to move; units
path round walls, block the flow, and get chewed if the swarm reaches them.
Walls block beams.

The economy runs on biomass: kills pay by species, digesters trickle it in,
every cleared wave pays a bonus plus 5% interest (up to 120), and Space calls
the next wave early for extra biomass. Spend it on E (evolve the selected
mouths, up to level 5), U (grow the great mouth: more range, more damage, a
second beam at level 3 and a third at 5), C (colony: +12% damage and +10% kill
reward for everything), H (repair) and Q (a free pulse that bursts everything
near the mouth, 40 s cooldown). There are 25 waves that grow to thousands of
blobs, then endless mode. P pauses and R restarts after a loss.

The seven sound effects are original synthesised CC0 clips; you can rebuild
them with `tools/make_tentacle_sfx.py`. Offscreen: `--screen tentacle` renders a
deterministic wave-18 battle. `--headless --check-tentacle` checks map
reachability, unit pathing, evolution, a full first wave, splitting, puddle
merging, a 3000-blob swarm against the walls, defeat and desktop routing.
