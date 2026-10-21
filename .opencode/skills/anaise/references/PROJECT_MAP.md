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
| `src/audio.h` | `anaise::Audio`: loads the three CC0 WAVs from RCDATA and streams them through `waveOut` on a worker thread, with crossfades, gapless loops and a mute ramp. |
| `src/ring_pattern.h` | `anaise::RingBackground`: orthogonal ring pattern drawn into a DIB, ordered-dithered to pure black and white, composited behind every screen except the splash. |
| `src/angel_art.h` | `anaise::kSeraphimArt`, the 114 x 57 Braille seraphim as one string_view. Data only, no code. |
| `src/app.rc` | Icon `101`, title font `102` (Squealer OTF as `RCDATA`), VERSIONINFO. |
| `assets/library.ico`, `assets/fonts/squealer.otf` | Generated 10x10 icon; CC0 typeface + licence. |
| `assets/audio/*.wav`, `assets/audio/CREDITS.txt` | Intro, menu and room loops, embedded as RCDATA 201-203; CC0 provenance and loop edits. |
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
Paint and Browser in independent windows; `screen_` tracks the focused app or
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

## Rendering rules (the part that gets broken)

- **Only `#000000` and `#FFFFFF`.** No greys, no alpha blending to grey. Softness
  is ordered dithering: the ring pattern uses a Bayer matrix in `dither()`, the
  pilgrim glow uses the same threshold trick on a blurred float field.
- **Everything after the splash sits on the ring background.** `paintInto()`
  draws `background_` first, then the screen; panels, room and canvas paint solid
  black over it so text stays legible.
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
| `--screen` | `splash` \| `menu` \| `library` \| `paint` \| `all` (default `all`). |
| `--size` | `WxH`, default `1080x740`. |
| `--time` | Virtual milliseconds, default `4200`; same value = same frame. |
| `--dpi` | DPI for font/layout choice, default `96`. |
| `--out` | Output directory, default `build\headless`. |
| `--walk` | `up\|down\|left\|right`; freezes a mid-step for the library screen. |
| `--progress` | `0..100` of that step, default `50`. |
| `--check-audio` | Parse the three embedded WAVs and exit (needs `--headless`); used by the test suite. |

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
L404   headless
L1407  window
L1727  browser
L1840  bethelament
L1941  mouse
L2295  veil
L2393  painting
L2513  fullscreen
L2544  drawing
L3268  bethelament art & layout
L3423  pilgrim light
L3547  room lens
L4070  veil
L4608  veil pages
L5121  members
```

### Symbols

```
L33    constexpr kLogoGrid
L37    constexpr kTitleFontResource
L57    constexpr kPaintCols
L58    constexpr kPaintRows
L61    constexpr kVeilPageCount
L65    constexpr kRoomCols
L66    constexpr kRoomRows
L67    constexpr kGameTimer
L68    constexpr kGameTickMs
L69    constexpr kStepMs
L70    constexpr kPoseMs
L71    constexpr kStandPose
L72    constexpr kPi
L75    enum Facing
L77    constexpr kDirX
L78    constexpr kDirY
L93    constexpr kWalkerW
L94    constexpr kWalkerH
L95    constexpr kWalkerHeadRows
L96    constexpr kWalkerBodyRows
L172   constexpr kSplashTimer
L173   constexpr kFrameTimer
L174   constexpr kFrameMs
L175   constexpr kSplashRevealMs
L177   constexpr kSplashTitleDelayMs
L178   constexpr kSplashHoldMs
L183   struct TitleFlash
L188   constexpr kTitleFlashes
L194   titleFlickerMs()
L201   litTitlePercent(ULONGLONG flickerElapsed)
L211   struct DotArt
L218   seraphimArt()
L224   constexpr kDotColumn
L225   constexpr kDotRow
L323   dpiForWindowFn()
L333   windowDpi(HWND hwnd)
L341   systemDpi()
L349   enableDpiAwareness()
L367   applyMonochromeTitleBar(HWND hwnd)
L390   loadEmbeddedFont(HINSTANCE instance)
L409   enum class Screen
L411   screenName(Screen screen)
L422   pngCrc32(const std::uint8_t* data, std::size_t size)
L441   appendBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
L457   pngAdler32(const std::uint8_t* data, std::size_t size)
L542   ensureDirectory(const std::wstring& path)
L559   struct HeadlessOptions
L578   splitCommandLine(const wchar_t* commandLine)
L601   parseNumber(const std::wstring& text, ULONGLONG& value)
L610   parseHeadlessOptions(const wchar_t* commandLine, HeadlessOptions& options)
L707   class App
L719     .create(HINSTANCE instance)
L784     .createHeadless(HINSTANCE instance, int width, int height, UINT dpi)
L861     .checkWindowInteractions()
L971   struct VeilSite
L1144    .appIconRows(int index)
L1225  enum class MenuPart
L1237  struct MenuHit
L1242  struct MenuLayout
L1265  struct AppWindow
L1272    .invalidate()
L1276    .windowAt(int x, int y)
L1285    .shellClient()
L1291    .appScreen(int index)
L1296    .activeApp()
L1301    .appWindowLayout(int index)
L1333    .contentClient(int index)
L1339    .contentPoint(int& x, int& y)
L1350    .focusApp(int index)
L1356    .dismissApp(bool close)
L1370  struct GridLayout
L1376  struct BrowserLayout
L1389  struct RoomLayout
L1395  struct Walker
L1423    .handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
L1626    .tickSplash()
L1635    .tickFrame()
L1643    .enterMenu()
L1661    .openScreen(Screen screen)
L1710    .activateMenuItem()
L1728    .browserGo(int page)
L1737    .browserFollowLink()
L1745    .browserReload()
L1752    .browserLoading()
L1757    .handleKey(int key)
L1821    .directionForKey(int key)
L1841    .roomWalkable(int col, int row)
L1849    .heldDirection()
L1867    .stopWalker()
L1879    .tickGame()
L1890    .updateWalker(double elapsed)
L1942    .onMouseMove(int x, int y)
L2038    .onMouseDown(bool leftButton, int x, int y)
L2151    .onMouseUp()
L2166    .menuHitAt(int x, int y)
L2205    .menuLayout(int width, int height)
L2289    .menuItemRect(int index, int width, int height)
L2296    .browserLayout(int width, int height)
L2363    .browserLinkRect(const BrowserLayout& layout)
L2375    .browserHitAt(int x, int y)
L2394    .paintLayout(int width, int height)
L2416    .paintCellAt(int x, int y, int& col, int& row)
L2428    .paintTo(int x, int y)
L2446    .stampBrush(int col, int row, bool erase)
L2461    .paintLine(int x0, int y0, int x1, int y1, bool erase)
L2482    .pushUndo()
L2489    .undoCanvas()
L2498    .redoCanvas()
L2507    .clearCanvas()
L2514    .setFullscreen(bool on)
L2547    .nowMs()
L2551    .releaseFonts()
L2563    .ensureFonts()
L2591    .textWidth(HDC dc, HFONT font, const std::wstring& text)
L2600    .textHeight(HDC dc, HFONT font)
L2641    .fillColor(HDC dc, const RECT& rect, COLORREF color)
L2684    .fillDither(HDC dc, const RECT& rect, int level)
L2783    .clockText()
L2792    .hot(MenuPart part, int index = -1)
L2799    .drawSigil(HDC dc, int cx, int cy, int radius)
L2839    .paintDesktopIcons(HDC dc, const MenuLayout& layout)
L2871    .paintAppWindow(HDC dc, const MenuLayout& layout, int index)
L2901  struct WindowButton
L2939    .paintDesktopApp(HDC dc, int index)
L2960    .paintTaskBar(HDC dc, const MenuLayout& layout)
L3027    .paintStartMenu(HDC dc, const MenuLayout& layout)
L3049    .paintBezel(HDC dc, const MenuLayout& layout, int width, int height)
L3135    .paint(HDC screen)
L3141    .paintInto(HDC screen, int width, int height)
L3165    .paintSplash(HDC dc, int width, int height)
L3236    .paintMenu(HDC dc, int width, int height)
L3269    .roomLayout(int width, int height)
L3308    .drawRoom(HDC dc, const RoomLayout& room)
L3391  struct WalkerGeometry
L3402    .walkerGeometry(const RoomLayout& room)
L3430    .walkerSourcePixel(int col, int row, int facing, int pose)
L3490    .ensureWalkerSurface(int width, int height)
L3529    .releaseWalkerSurface()
L3552    .ensureRoomSurface(int width, int height)
L3587    .releaseRoomSurface()
L3603    .bendRoom(int centerX, int centerY, int radius, double strength)
L3628  constexpr kRippleSteps
L3723    .drawWalker(HDC dc, const RoomLayout& room)
L3911    .paintLibrary(HDC dc, int width, int height)
L3951    .paintPainterWallpaper(HDC dc, int width, int height)
L4012    .paintPaintScreen(HDC dc, int width, int height)
L4071    .browserNoise(int x, int y, int seed)
L4190    .drawMarquee(HDC dc, const RECT& bar, const std::wstring& message)
L4248    .drawPixel404(HDC dc, const RECT& page)
L4264    .paintGamePage(HDC dc, const RECT& page, bool compact)
L4358    .paintBaelPage(HDC dc, const RECT& page, bool compact)
L4428    .paintOrderPage(HDC dc, const RECT& page, bool compact)
L4508    .paintGoatPage(HDC dc, const RECT& page, bool compact)
L4563    .paint404Page(HDC dc, const RECT& page, bool compact)
L4612    .px(int value)
L4640    .factBoxHeight(HDC dc, const wchar_t* facts)
L4650    .drawFactBox(HDC dc, const wchar_t* facts, const RECT& box)
L4709    .drawHitCounter(HDC dc, int x, int y, int value, int digits)
L4861    .paintGuestbookPage(HDC dc, const RECT& page, bool compact)
L4941    .paintBrowserLoading(HDC dc, const BrowserLayout& layout)
L4972    .paintBrowserLink(HDC dc, const BrowserLayout& layout)
L4994    .paintBrowser(HDC dc, int width, int height)
L5209  runHeadless(HINSTANCE instance, const HeadlessOptions& options)
L5243  wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int)
```
<!-- map:end -->
