# Anaise project map

Working notes for an agent: read this before opening source. It is one page that
says where everything lives, what must not break, and how to check work without
a window. Line numbers at the bottom are generated and go stale; the anchor text
next to them does not.

Refresh the generated block with:

```bat
powershell -ExecutionPolicy Bypass -File .opencode\skills\anaise\scripts\map.ps1
```

## Files

| Path | Role |
| --- | --- |
| `src/main.cpp` | Everything except the ring: Win32 window, message loop, splash, menu, Bethelament room, Painter, the headless renderer and the PNG writer. One anonymous namespace plus `class App`. ~3.3k lines. |
| `src/audio.h` | `anaise::Audio`: four CC0 music clips, six CC0 effects, eight overlapping effect voices, hymn pitch/distortion, crossfades and global mute; worker-thread `waveOut` streaming. |
| `src/new_gods.h` | `anaise::NewGods`: rolling click ritual, particles, cinematic, software 3D arena and combat, four-floor offline tower story; deterministic `selfTest()`. Desktop routing and live input stay in `App`. |
| `src/ring_pattern.h` | `anaise::RingBackground`: orthogonal ring pattern drawn into a DIB, ordered-dithered to pure black and white, composited behind every screen except the splash. |
| `src/school_horror.h` | Fictional adult college cast: knife inventory, attack tests, damage, panic, escape paths and blood event state. |
| `src/angel_art.h` | `anaise::kSeraphimArt`, the 114 x 57 Braille seraphim as one string_view. Data only, no code. |
| `src/app.rc` | Icon `101`, title font `102` (Squealer OTF as `RCDATA`), VERSIONINFO. |
| `assets/library.ico`, `assets/fonts/squealer.otf` | Generated 10x10 icon; CC0 typeface + licence. |
| `assets/audio/*.wav`, `assets/audio/CREDITS.txt` | Music resources 201-205, SFX 211-218; CC0 provenance and edits. `tools/prepare_gods_audio.py`, `prepare_gods_sfx.py` and `prepare_school_audio.py` rebuild audio with development-only Python libraries. |
| `tools/test.ps1` | Headless test suite: builds, renders, checks PNGs. The only test entry point. |
| `tools/headless.ps1` | Convenience wrapper around `bin\Anaise.exe --headless`. |
| `tools/capture.ps1` | Grabs a *running* window with `PrintWindow` (no activate/raise). Not used by tests. |
| `tools/make_icon.ps1`, `tools/make_shortcut.ps1` | Regenerate the icon; drop a desktop shortcut. |
| `build.bat`, `run.bat`, `test.bat` | MinGW-w64 build (static CRT, `-municode -mwindows`), run, tests. |
| `README.md` | Public description of the app and its rules. Keep it in sync with behaviour. |
| `AGENTS.md` | Project rules: public repo, no secrets, headless only. |

## How main.cpp is laid out

1. **Constants and data** — logo grid, paint grid 32x32, room 13x9, timers
   (splash 1, game 2 at 15 ms, frame 3 at 40 ms), step timing `kStepMs` 240,
   `kPoseMs` 120, facings and deltas, walker sprite rows, splash timings.
2. **Splash data** — `TitleFlash` frames, `titleFlickerMs()`, `litTitlePercent()`,
   `DotArt` + `seraphimArt()` (decodes Braille bit masks into a dot list and a
   reveal order sorted from the centre outward).
3. **Win32 helpers** — `enableDpiAwareness()`, `applyMonochromeTitleBar()`.
4. **Headless layer** — `Screen` enum, CRC/Adler PNG encoder (`writePng`),
   `ensureDirectory()`, `HeadlessOptions`, command-line splitting, `parseHeadlessOptions()`.
5. **`class App`** — the whole app (see below).
6. **Entry points** — `runHeadless()` (offscreen PNGs) and `wWinMain()` (ordinary
   launch ignores its arguments; only `--headless` changes behaviour).

`App` is grouped by banner comments in this order:

| Group | Contents |
| --- | --- |
| creation | `create()` (register window, timers, fonts, message loop), `createHeadless()` (no window, fixed client size, virtual clock). |
| menu data | `kMenuTitles` / `Kinds` / `Blurbs` / `Hints`, `appIconRows()` 16x16 glyphs, `MenuLayout`, `GridLayout`, `RoomLayout`, `Walker` structs. |
| window | `windowProc()`, `handleMessage()`: WM_PAINT, keys, mouse, WM_TIMER, focus loss, DPI change, fullscreen keys. |
| screen flow | `tickSplash()`, `tickFrame()`, `enterMenu()`, `openScreen()`, `activateMenuItem()`, `handleKey()`, `directionForKey()`. |
| bethelament | `roomWalkable()` (walls/shelves bitmask), `heldDirection()`, `stopWalker()`, `tickGame()`, `updateWalker()` (1-2-3-2 step cycle, one tile per press, last-pressed wins). |
| mouse | `onMouseMove/Down/Up`, `menuIndexAt()`, `menuLayout()`, `menuItemRect()`. |
| painting | `paintLayout()`, `paintCellAt()`, `paintTo()`, `stampBrush()` (square brush 1-3), `paintLine()` (Bresenham), `pushUndo()`, `undoCanvas()`, `clearCanvas()`. |
| fullscreen | `setFullscreen()` (style + `WINDOWPLACEMENT` round trip). |
| drawing | `nowMs()` (virtual clock when headless), font cache, `textWidth/Height`, `drawTextAt()` / `drawTextCentered()` / `drawSplashTitle()` (clip-reveal title), `fillColor`, `strokeRect`, `drawLine`, `drawLogo`, `paint()` -> `paintInto()` (ring background, then screen dispatch), `paintSplash()`, `paintMenu()`. |
| bethelament art | `roomLayout()`, `drawRoom()` (shelves, book spines), `walkerGeometry()`. |
| pilgrim light | `walkerSourcePixel()` silhouette, separable box blur into `walkerLight_`, `walkerTrail_`, `walkerGlow_`, DIB surfaces, `ensure/releaseWalkerSurface()`. |
| room lens | `ensureRoomSurface()`, `bendRoom()` (nearest-neighbour magnify/twist/ripple around the pilgrim), `drawWalker()`, `paintLibrary()`. |
| painter screen | `paintPaintScreen()` (grid, hover outline, brush/HUD text), `paintInto()` tail. |
| members | all state: `hwnd_`, fonts, `screen_`, `splashStart_`, `menuIndex_`, `anaise::RingBackground background_`, walker surfaces and light fields, canvas, undo stack, brush, hover, fullscreen, DPI. |

## Screen flow

`Splash` -> `Menu` desktop, initially with no open apps. Icons launch Library,
Paint, Browser and Snake in independent windows; `screen_` tracks the focused app or
Menu when the desktop is focused. `appWindows_` retains open/minimized/maximized
state and bounds; `windowOrder_` stores back-to-front stacking. Content renders
offscreen, then copies into the window body; `contentPoint()` maps mouse input
back into that content surface. Taskbar tabs restore/switch or minimize the
active app. Title controls close only their app, maximize/restore, or minimize;
titles drag with capture. Desktop icons also drag with capture; a short press
launches on release, while a drag only changes the session's icon position.
`iconPositions_` stores DPI-independent offsets, clamped by `menuLayout()`.
The top bar and programs panel are gone; the clock and speaker are on the lower
bar. Painter tiles RTL Hebrew lettering outside its opaque canvas.
`Esc` minimizes the app. Native fullscreen uses
`F11` / `Alt+Enter`; `Esc` leaves fullscreen first.

`--check-windows` exercises window lifecycle, stacking, dragging, resizing and
Painter/Veil input routing entirely offscreen. Headless screen captures open
only the requested app; the menu capture has no open apps.

After all four 2D puzzle rooms, the breach leads through Crash and Search into
`GamePhase::Corridor`: After School, a 31x25 first-person school dating sim.
`kSchoolGirls` and `kSchoolTalks` define four club girls and their dialogue.
`corridorSolid()` defines classroom partitions, doors and desks;
`moveCorridor()` handles collision; `schoolTarget()` checks interaction range
and visibility. `interactSchool()` / `chooseSchoolReply()` handle conversations,
hearts, rizz, invitations and endings. `paintCorridor()` draws the HUD/map/dialogue;
`renderCorridor()` draws the world and depth-tested sprites (`schoolGirlPixel()`).
W/S walk, A/D turn, E talks, 1-3 answer, Backspace leaves dialogue, R restarts.
Esc minimizes and preserves school progress. Enter on a date ending resets 2D.
`--headless --screen corridor --school-view 0|1|2` captures exploration, dialogue,
or an ending. `--check-game` checks both 2D progression and school interactions.

The college cast consists of four entirely fictional adults aged 25-27; the
former real-person photographs were replaced before adding horror mechanics.
`assets/school/CREDITS.md` records adult character designs and imagegen prompts;
`masters/` retains generated neutral crops and six-expression color sheets.
`tools/make_school_faces.py` imports these into 1536x256 luminance BMP atlases,
embedded as resources 401-404. `ensureSchoolFaces()` caches decoded pixels;
`schoolExpression()` selects timed reactions; `drawSchoolPortrait()` draws the
dialogue portrait; `renderCorridor()` samples the same face onto the world sprite.
Both apply Bayer dithering after sizing, so rendered pixels remain binary.
Headless `--school-girl 1..4 --school-expression neutral|happy|shy|hurt|afraid|dead` selects
preview identities and reactions. `--check-game` validates all 24 texture
cells and the reaction logic; `test.ps1` renders all four girls offscreen.

`src/school_horror.h` owns session-only knife discovery/equip, three-hit damage,
attack reach/cone/wall tests, panic, stunned/wounded movement, escape routing,
corpses, sound events and capped blood bursts. E searches, Q equips, F/click
attacks, Shift sprints. `paintSchoolViolence()` draws particles and the blade;
world rendering adds floor pools, blood stains and horizontal bodies. There are
no emotion labels or club suffixes in character name tags. School views 3/4
capture panic and aftermath. Resources 205 and 217-218 supply CC0 Cate music
and two acted screams; source records are in assets/audio/CREDITS.txt.

## Snake

`src/snake.h` owns the session-only 26x18 drill-snake game, deterministic target
placement, turn queue, scoring/speed, collisions, pause, and face reactions.
Desktop app index 3 is Snake; index 4 is Shutdown. App state arrays and taskbar
loops use `kAppCount` (4), while icon/start-menu loops use `kMenuItems` (5).
`assets/snake/` contains three Met Open Access CC0 portraits, source metadata,
credits and imported luminance BMPs (resources 501-503).
`tools/make_snake_faces.py` rebuilds crops offline. Runtime dithering plus a
binary final composite on the focused Snake screen preserve the palette.
Enter starts/retries, arrows/WASD steer, Space/P pauses, R resets; losing focus
pauses play. `--screen snake --snake-view 1 --time 800` previews eye-roll/drool;
`--check-snake` checks game logic, embedded faces, and desktop routing.
A hit enters a 3600ms cinematic: 0-500ms zoom/contact; one acted CC0 grunt at
500ms; eye-roll/drool reaction; next-person/level reveal at 2900ms; new target
and level at 3600ms. Movement and turn input freeze during this sequence;
pause/focus loss freezes both animation and the voice. `takeGroan()` emits one
cue per hit; `Audio::snakeGroan()` routes it to resources 219-221. All audio
obeys M. `--snake-view 1 --time 100|800|3200|4000` previews the timeline, including
the resumed next-level board. Source/edits: `assets/audio/CREDITS.txt` and
`tools/prepare_snake_audio.py`.

## Rendering rules (the part that gets broken)

- **Only `#000000` and `#FFFFFF`.** No greys, no alpha blending to grey. Softness
  is ordered dithering: the ring pattern uses a Bayer matrix in `dither()`, the
  pilgrim glow uses the same threshold trick on a blurred float field.
- **Everything after the splash sits on the ring background.** `paintInto()`
  draws `background_` first, then the screen; panels, room and canvas paint solid
  black over it so text stays legible. The break and The New Gods take over
  the full scene with opaque black backgrounds.
- **Headless must stay deterministic.** In headless mode `nowMs()` returns the
  virtual clock and no timer runs, so every frame is a pure function of `--time`.
  Don't add randomness, and don't let a headless code path read the wall clock
  (live code uses `GetTickCount64()` in a few spots: splash start/hold, game tick).
- **Static CRT, no runtime bundle, no network.** Don't add dependencies.
- **DPI:** per-monitor; fonts are recreated when `fontsDpi_` changes and the DIB
  surfaces are sized in device pixels.

## Headless renderer

```bat
bin\Anaise.exe --headless --screen all --size 1080x740 --time 4200 --out build\headless
bin\Anaise.exe --headless --screen library --walk right --progress 50 --out build\headless
```

| Flag | Meaning |
| --- | --- |
| `--headless` | Required to enter the offscreen mode; ordinary launches ignore arguments. |
| `--screen` | `splash` \| `menu` \| `library` \| `paint` \| `browser` \| `crash` \| `search` \| `corridor` \| `gods-ritual` \| `gods-arena` \| `gods-tower` \| `all` (default `all`). |
| `--size` | `WxH`, default `1080x740`. |
| `--time` | Virtual milliseconds, default `4200`; same value = same frame. |
| `--dpi` | DPI for font/layout choice, default `96`. |
| `--out` | Output directory, default `build\headless`. |
| `--walk` | `up\|down\|left\|right`; freezes a mid-step for the library screen. |
| `--progress` | `0..100` of that step, default `50`. |
| `--check-audio` | Parse music/SFX and exercise pitch ascent, fade, mixing and mute without a device. |
| `--check-gods` | Exercise ritual timing, title hit routing, combat, pause, collision, retries and the full tower chapter including both endings. |
| `--gods-view` | `0` default; `1..10` rupture, Athena, Zeus, admission, memory, Warden, final choice, ending, defeat and dialogue. `--time` sets stage animation age; reveal caption impacts at 3600 ms. |

Bad values exit non-zero with a usage message, and `test.ps1` asserts that.

## Tests

`powershell -ExecutionPolicy Bypass -File tools\test.ps1` (add `-NoBuild` to skip
the build). It renders offscreen only, then checks:

- every screen renders at the requested size and contains only black and white;
- a fixed `--time` reproduces byte-identical PNGs across two runs;
- the splash is nearly empty early and fully drawn and titled later;
- a frozen mid-step library frame carries the smear/halo and stays in palette;
- the embedded music parses in a headless run (`--check-audio`);
- nonsense `--screen` and `--walk` values are rejected.

When behaviour changes, extend `tools/test.ps1` in the same style: one `Check`
line per assertion, no external image tooling.

## Code graph (graphify)

`graphify-out/` is generated, offline and gitignored (see the skill body for the
commands). It is useful here for one thing: `explain` / `affected` on a symbol,
which is cheaper than re-reading the 3.3k-line file. Prose questions are a poor
fit at this size; the map above beats a BFS over 342 nodes.

<!-- map:begin -->
<!-- generated by scripts/map.ps1; do not edit by hand -->

### Sections in `src/main.cpp`

```
L570   headless
L2064  window
L2483  browser
L2773  bethelament
L3066  mouse
L3457  veil
L3551  painting
L3671  fullscreen
L3702  drawing
L4468  bethelament art & layout
L5098  pilgrim light
L5222  room lens
L5774  the break
L6871  veil
L7383  veil pages
L7978  members
```

### Symbols

```
L37    constexpr kLogoGrid
L41    constexpr kTitleFontResource
L47    constexpr kBlackletterFontResource
L68    constexpr kPaintCols
L69    constexpr kPaintRows
L72    constexpr kVeilPageCount
L84    constexpr kRoomCols
L85    constexpr kRoomRows
L86    constexpr kRoomCount
L87    constexpr kGameTimer
L88    constexpr kGameTickMs
L89    constexpr kStepMs
L90    constexpr kPoseMs
L91    constexpr kStandPose
L92    constexpr kPi
L95    enum Facing
L97    constexpr kDirX
L98    constexpr kDirY
L101   enum DoorLock
L104   enum RoomStyle
L154   struct RoomDef
L163   constexpr kRooms
L177   struct RoomDoor
L197   constexpr kLeverCount
L198   constexpr kLeverTarget
L202   constexpr kCrashAutoMs
L203   constexpr kSearchAutoMs
L204   constexpr kCorridorBootMs
L207   constexpr kCorridorCols
L208   constexpr kCorridorRows
L210   enum class SchoolExpression
L211   constexpr kSchoolFaceSize
L212   constexpr kSchoolFaceExpressions
L214   struct SchoolGirl
L221   constexpr kSchoolGirls
L227   struct SchoolTalk
L259   constexpr kWalkerW
L260   constexpr kWalkerH
L261   constexpr kWalkerHeadRows
L262   constexpr kWalkerBodyRows
L338   constexpr kSplashTimer
L339   constexpr kFrameTimer
L340   constexpr kFrameMs
L341   constexpr kSplashRevealMs
L343   constexpr kSplashTitleDelayMs
L344   constexpr kSplashHoldMs
L349   struct TitleFlash
L354   constexpr kTitleFlashes
L360   titleFlickerMs()
L367   litTitlePercent(ULONGLONG flickerElapsed)
L377   struct DotArt
L384   seraphimArt()
L390   constexpr kDotColumn
L391   constexpr kDotRow
L489   dpiForWindowFn()
L499   windowDpi(HWND hwnd)
L507   systemDpi()
L515   enableDpiAwareness()
L533   applyMonochromeTitleBar(HWND hwnd)
L556   loadEmbeddedFont(HINSTANCE instance, int resourceId)
L575   enum class Screen
L592   screenName(Screen screen)
L612   enum class GamePhase
L614   pngCrc32(const std::uint8_t* data, std::size_t size)
L633   appendBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
L649   pngAdler32(const std::uint8_t* data, std::size_t size)
L734   ensureDirectory(const std::wstring& path)
L751   struct HeadlessOptions
L783   splitCommandLine(const wchar_t* commandLine)
L806   parseNumber(const std::wstring& text, ULONGLONG& value)
L815   parseHeadlessOptions(const wchar_t* commandLine, HeadlessOptions& options)
L987   class App
L1005    .create(HINSTANCE instance)
L1072    .createHeadless(HINSTANCE instance, int width, int height, UINT dpi)
L1211    .checkSnakeInteractions()
L1227    .checkGodsInteractions()
L1249    .checkWindowInteractions()
L1339    .checkBrowserInteractions()
L1412    .checkGameInteractions()
L1608  struct VeilSite
L1781    .appIconRows(int index)
L1869  enum class MenuPart
L1881  struct MenuHit
L1886  struct MenuLayout
L1909  struct AppWindow
L1916    .invalidate()
L1920    .windowAt(int x, int y)
L1929    .shellClient()
L1935    .appScreen(int index)
L1940    .activeApp()
L1951    .appWindowLayout(int index)
L1983    .contentClient(int index)
L1989    .contentPoint(int& x, int& y)
L2000    .focusApp(int index)
L2006    .dismissApp(bool close)
L2021  struct GridLayout
L2027  struct BrowserTab
L2033  struct BrowserLayout
L2046  struct RoomLayout
L2052  struct Walker
L2080    .handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
L2328    .tickSplash()
L2337    .tickFrame()
L2365    .enterMenu()
L2385    .openScreen(Screen screen)
L2463    .activateMenuItem()
L2484    .browserGo(int page)
L2506    .browserHistory(int delta)
L2517    .browserSelectTab(int index)
L2528    .browserNewTab()
L2535    .browserCloseTab(int index)
L2547    .browserEditAddress()
L2554    .browserPasteAddress()
L2571    .browserLower(std::wstring value)
L2576    .browserRows()
L2588    .browserSubmitAddress()
L2602    .browserCharacter(wchar_t c)
L2615    .browserScroll(int delta)
L2625    .browserFollowLink()
L2633    .browserReload()
L2640    .browserLoading()
L2645    .handleKey(int key)
L2754    .directionForKey(int key)
L2774    .roomTile(int room, int col, int row)
L2782    .solidTile(char tile)
L2788    .doorAt(int room, int col, int row)
L2796    .doorUnlocked(const RoomDoor& door)
L2805    .roomWalkable(int col, int row)
L2813    .setNotice(const wchar_t* text)
L2819    .resetRunState()
L2832    .resetGame()
L2845    .enterRoom(int room, int spawnX, int spawnY)
L2862    .onArrive()
L2910    .leverIndexAt(int col, int row)
L2923    .toggleLever(int col, int row)
L2940    .heldDirection()
L2958    .stopWalker()
L2970    .tickGame()
L3002    .updateWalker(double elapsed)
L3067    .onMouseMove(int x, int y)
L3164    .onMouseDown(bool leftButton, int x, int y)
L3312    .onMouseUp()
L3328    .menuHitAt(int x, int y)
L3367    .menuLayout(int width, int height)
L3451    .menuItemRect(int index, int width, int height)
L3458    .browserLayout(int width, int height)
L3510    .browserLinkRect(const BrowserLayout& layout)
L3522    .browserHitAt(int x, int y)
L3552    .paintLayout(int width, int height)
L3574    .paintCellAt(int x, int y, int& col, int& row)
L3586    .paintTo(int x, int y)
L3604    .stampBrush(int col, int row, bool erase)
L3619    .paintLine(int x0, int y0, int x1, int y1, bool erase)
L3640    .pushUndo()
L3647    .undoCanvas()
L3656    .redoCanvas()
L3665    .clearCanvas()
L3672    .setFullscreen(bool on)
L3703    .godsTitleRect()
L3714    .godsTitleHit(int x,int y)
L3724    .nowMs()
L3728    .releaseFonts()
L3742    .ensureFonts()
L3773    .textWidth(HDC dc, HFONT font, const std::wstring& text)
L3782    .textHeight(HDC dc, HFONT font)
L3878    .fillColor(HDC dc, const RECT& rect, COLORREF color)
L3921    .fillDither(HDC dc, const RECT& rect, int level)
L4013    .clockText()
L4022    .hot(MenuPart part, int index = -1)
L4029    .drawSigil(HDC dc, int cx, int cy, int radius)
L4069    .paintDesktopIcons(HDC dc, const MenuLayout& layout)
L4089    .paintAppWindow(HDC dc, const MenuLayout& layout, int index)
L4119  struct WindowButton
L4157    .paintDesktopApp(HDC dc, int index)
L4179    .paintTaskBar(HDC dc, const MenuLayout& layout)
L4246    .paintStartMenu(HDC dc, const MenuLayout& layout)
L4268    .paintBezel(HDC dc, const MenuLayout& layout, int, int)
L4295    .paint(HDC screen)
L4301    .paintInto(HDC screen, int width, int height)
L4355    .paintSplash(HDC dc, int width, int height)
L4426    .paintMenu(HDC dc, int width, int height)
L4469    .roomLayout(int width, int height)
L4510    .drawCandle(HDC dc, int left, int top, int cell, int seed)
L4644    .drawKeyTile(HDC dc, int left, int top, int cell)
L4667    .drawGrave(HDC dc, int left, int top, int cell)
L4686    .drawPillar(HDC dc, int left, int top, int cell)
L4703    .drawBreach(HDC dc, int left, int top, int cell)
L4744    .drawDoorway(HDC dc, const RoomLayout& room, const RoomDoor& door)
L4802    .drawRoom(HDC dc, const RoomLayout& room)
L5038  struct GhostSpot
L5066  struct WalkerGeometry
L5077    .walkerGeometry(const RoomLayout& room)
L5105    .walkerSourcePixel(int col, int row, int facing, int pose)
L5165    .ensureWalkerSurface(int width, int height)
L5204    .releaseWalkerSurface()
L5227    .ensureRoomSurface(int width, int height)
L5262    .releaseRoomSurface()
L5279    .ensureBreakSurface(int width, int height)
L5311    .releaseBreakSurface()
L5326    .ensureWorldSurface(int width, int height)
L5359    .releaseWorldSurface()
L5375    .bendRoom(int centerX, int centerY, int radius, double strength)
L5400  constexpr kRippleSteps
L5495    .drawWalker(HDC dc, const RoomLayout& room)
L5683    .paintLibrary(HDC dc, int width, int height)
L5779    .glitchHash(int x, int y, int seed)
L5804    .startBreak()
L5814    .beginSearch()
L5821    .beginCorridor()
L5843    .endBreak()
L5849    .paintBreak(HDC dc, int width, int height)
L5860    .paintCrash(HDC dc, int width, int height)
L5928    .drawCrashDialog(HDC dc, int width, int height, double t)
L6018    .paintSearch(HDC dc, int width, int height)
L6112    .corridorSolid(int x, int y)
L6123    .updateCorridor(double elapsed)
L6138    .moveCorridor(double elapsed, double move, double turn)
L6147  constexpr kRadius
L6163    .schoolTarget()
L6182    .flushSchoolSounds()
L6191    .attackSchool()
L6204    .paintSchoolViolence(HDC dc, int width, int height)
L6247    .interactSchool()
L6261    .chooseSchoolReply(int choice)
L6296    .ensureSchoolFaces()
L6325    .schoolExpression(int girl)
L6333    .drawSchoolPortrait(HDC dc, int girl, const RECT& bounds)
L6360    .drawSchoolHearts(HDC dc, int x, int y, int hearts)
L6372    .paintSchoolMap(HDC dc)
L6391    .paintCorridor(HDC dc, int width, int height)
L6394  struct CrispFont
L6483    .renderCorridor(HDC dc, int width, int height)
L6715    .schoolGirlPixel(int x, int y, int girl)
L6752    .paintPainterWallpaper(HDC dc, int width, int height)
L6813    .paintPaintScreen(HDC dc, int width, int height)
L6872    .browserNoise(int x, int y, int seed)
L6991    .drawMarquee(HDC dc, const RECT& bar, const std::wstring& message)
L7049    .drawPixel404(HDC dc, const RECT& page)
L7065    .paintGamePage(HDC dc, const RECT& page, bool compact)
L7142    .paintBaelPage(HDC dc, const RECT& page, bool compact)
L7208    .paintOrderPage(HDC dc, const RECT& page, bool compact)
L7286    .paintGoatPage(HDC dc, const RECT& page, bool compact)
L7338    .paint404Page(HDC dc, const RECT& page, bool compact)
L7387    .px(int value)
L7415    .factBoxHeight(HDC dc, const wchar_t* facts)
L7425    .drawFactBox(HDC dc, const wchar_t* facts, const RECT& box)
L7484    .drawHitCounter(HDC dc, int x, int y, int value, int digits)
L7608    .paintGuestbookPage(HDC dc, const RECT& page, bool compact)
L7685    .paintBrowserLoading(HDC dc, const BrowserLayout& layout)
L7710    .paintBrowserLink(HDC dc, const BrowserLayout& layout)
L7732    .browserFaceResource(int seed)
L7766    .drawBrowserWatcher(HDC dc, int cx, int cy, int size, int seed)
L7783    .paintBrowserPanel(HDC dc, const BrowserLayout& layout)
L7811    .paintBrowserRemains(HDC dc, const RECT& page)
L7837    .paintBrowser(HDC dc, int width, int height)
L8128  runHeadless(HINSTANCE instance, const HeadlessOptions& options)
L8169  wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int)
```
<!-- map:end -->
