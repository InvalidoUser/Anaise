// Anaise -- a small black-and-white library.
//
// A native Win32 app with a dot-art seraphim startup screen, a main menu, the
// quiet Bethelament room, a Painter canvas, and Veil: a browser that only
// reaches dead cult pages. The palette is strictly black and white and nothing
// is written to disk.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "angel_art.h"
#include "audio.h"
#include "school_horror.h"
#include "new_gods.h"
#include "snake.h"
#include "tentacle.h"
#include "occult_art.h"
#include "ring_pattern.h"

namespace {

constexpr int kLogoGrid = 10;

// Title typeface: Squealer by Raymond Larabie (Typodermic Fonts), released
// under CC0 1.0. See assets/fonts/squealer-LICENSE.txt.
constexpr int kTitleFontResource = 102;
const wchar_t* const kTitleFace = L"Squealer";

// Desktop dedication typeface: UnifrakturMaguntia, a blackletter/Old English
// face under the SIL Open Font License 1.1. See
// assets/fonts/unifrakturmaguntia-OFL.txt.
constexpr int kBlackletterFontResource = 103;
const wchar_t* const kBlackletterFace = L"UnifrakturMaguntia";
const wchar_t* const kDesktopDedication = L"The New Gods";

// The 10x10 logo: three books on a shelf, white on black.
const wchar_t* const kLogo[kLogoGrid] = {
    L"..........",
    L".##..##..#",
    L".##..##..#",
    L".##..##..#",
    L".##..##..#",
    L".##..##..#",
    L".##..##..#",
    L"##########",
    L"..........",
    L"..........",
};

const COLORREF kBlack = RGB(0, 0, 0);
const COLORREF kWhite = RGB(255, 255, 255);

constexpr int kPaintCols = 32;
constexpr int kPaintRows = 32;

// Veil is a small dead web: fifteen addresses, most of them still up.
constexpr int kVeilPageCount = 15;

// Bethelament is a fixed tile grid, the way an RPG Maker map is: one step per
// tile, four facings, and a three-frame walk cycle that runs 1-2-3-2.
//
// The game is four rooms in a chain, each holding one door shut with its own
// kind of lock:
//
//   #  wall             .  floor             B  bookcase / pillar / tomb
//   D  door (locked)    d  door (open)       K  key on the floor
//   L  lever            1-3 numbered plate   O  the breach
//   ~  unstable floor   G  grave niche       C  candle stand
constexpr int kRoomCols = 13;
constexpr int kRoomRows = 9;
constexpr int kRoomCount = 4;
constexpr UINT_PTR kGameTimer = 2;
constexpr int kGameTickMs = 15;
constexpr double kStepMs = 240.0;
constexpr double kPoseMs = 120.0;
constexpr int kStandPose = 1;
constexpr double kPi = 3.14159265358979323846;

// Rows are ordered the way an RPG Maker charset is: down, left, right, up.
enum Facing { kFaceDown = 0, kFaceLeft = 1, kFaceRight = 2, kFaceUp = 3 };

constexpr int kDirX[4] = {0, -1, 1, 0};
constexpr int kDirY[4] = {1, 0, 0, -1};

// What a door wants before it opens.
enum DoorLock { kDoorOpen = 0, kDoorKey, kDoorLevers, kDoorPlates };

// Room styles drive the decoration: reading room, cloister, ossuary, breach.
enum RoomStyle { kStyleLibrary = 0, kStyleCloister, kStyleOssuary, kStyleBreach };

const wchar_t* const kRoomMapBethelament[kRoomRows] = {
    L"######D######",
    L"#...........#",
    L"#.B.......B.#",
    L"#.....K.....#",
    L"#...........#",
    L"#...........#",
    L"#.B.......B.#",
    L"#...........#",
    L"#############",
};

const wchar_t* const kRoomMapCloister[kRoomRows] = {
    L"#####D#######",
    L"#...........#",
    L"#..L..L..L..#",
    L"#...........#",
    L"#.B.......B.#",
    L"#...........#",
    L"#...........#",
    L"#...........#",
    L"######d######",
};

const wchar_t* const kRoomMapOssuary[kRoomRows] = {
    L"####D########",
    L"#.C.......C.#",
    L"#.G.G...G.G.#",
    L"#...........#",
    L"#..1.....2..#",
    L"#...........#",
    L"#.....3.....#",
    L"#...........#",
    L"######d######",
};

const wchar_t* const kRoomMapBreach[kRoomRows] = {
    L"#############",
    L"#...........#",
    L"#.B..~..~.B.#",
    L"#...........#",
    L"#.....O.....#",
    L"#...........#",
    L"#.B...~...B.#",
    L"#...........#",
    L"######d######",
};

struct RoomDef {
    const wchar_t* const* rows;
    const wchar_t* title;
    const wchar_t* hint;
    int style;
    int startX;
    int startY;
};

constexpr RoomDef kRooms[kRoomCount] = {
    {kRoomMapBethelament, L"ROOM I - BETHELAMENT",
     L"A key lies on the floor. Step on it to unbar the north door.",
     kStyleLibrary, 6, 6},
    {kRoomMapCloister, L"ROOM II - THE CLOISTER",
     L"Set the three levers to match the carvings. Face one and press E.",
     kStyleCloister, 6, 7},
    {kRoomMapOssuary, L"ROOM III - THE OSSUARY",
     L"Step on the plates in order: I, then II, then III.",
     kStyleOssuary, 6, 7},
    {kRoomMapBreach, L"ROOM IV - THE BREACH",
     L"The wall is wrong here. Something is repeating.", kStyleBreach, 6, 7},
};

struct RoomDoor {
    int x = -1;
    int y = -1;
    int target = 0;      // room reached
    int spawnX = 0;      // where the pilgrim lands in that room
    int spawnY = 0;
    int lock = kDoorOpen;  // DoorLock
};

// Every room's doorways. The first is the way back; the second, when present,
// is the door the room's puzzle holds shut.
constexpr RoomDoor kRoomDoors[kRoomCount][2] = {
    {{6, 0, 1, 6, 7, kDoorKey}, {-1, -1, 0, 0, kDoorOpen}},
    {{6, 8, 0, 6, 1, kDoorOpen}, {5, 0, 2, 6, 7, kDoorLevers}},
    {{6, 8, 1, 5, 1, kDoorOpen}, {4, 0, 3, 6, 7, kDoorPlates}},
    {{6, 8, 2, 4, 1, kDoorOpen}, {-1, -1, 0, 0, kDoorOpen}},
};

// The cloister levers must read up, down, up. The pattern is carved over each
// lever as a small arrow, so the puzzle is entirely inside the room.
constexpr int kLeverCount = 3;
constexpr bool kLeverTarget[kLeverCount] = {true, false, true};

// The break: the game falls over, the machine searches for an answer, and then
// there is a corridor. Timings are fixed so a headless capture is repeatable.
constexpr ULONGLONG kCrashAutoMs = 7200;
constexpr ULONGLONG kSearchAutoMs = 6800;
constexpr ULONGLONG kCorridorBootMs = 1100;

// The corridor is a first-person grid: '#' is a wall, '.' is floor.
constexpr int kCorridorCols = 31;
constexpr int kCorridorRows = 25;

enum class SchoolExpression { Neutral, Happy, Shy, Hurt, Afraid, Dead };
constexpr int kSchoolFaceSize = 256;
constexpr int kSchoolFaceExpressions = 6;

struct SchoolGirl {
    const wchar_t* name;
    const wchar_t* club;
    double x, y;
    const wchar_t* reply;
    const wchar_t* date;
};
constexpr SchoolGirl kSchoolGirls[] = {
    {L"HANA", L"ART STUDIO", 5.5, 7.5, L"You noticed the little details. I like that.", L"Hana saves you a seat at the festival's portrait booth."},
    {L"REI", L"GYM", 25.5, 7.5, L"Okay, we're a pretty good team!", L"Rei takes your hand and races you to the festival."},
    {L"YUMI", L"LIBRARY", 5.5, 17.5, L"It's easy to talk when someone actually listens.", L"Yumi shares her favorite book under the festival lights."},
    {L"MIKA", L"MUSIC ROOM", 25.5, 17.5, L"You make the stage feel a little less scary.", L"Mika dedicates the festival's last song to you."},
};
struct SchoolTalk {
    const wchar_t* question;
    const wchar_t* choices[3];
    int best;
};
constexpr SchoolTalk kSchoolTalks[4][3] = {
    {{L"I'm painting the festival poster. What do you think?", {L"The tiny stars are beautiful.", L"Just copy someone else's.", L"It looks nice."}, 0},
     {L"I can't decide what to paint next.", {L"Whatever gets the most likes.", L"Show me a place you love.", L"Maybe a landscape?"}, 1},
     {L"Would you help me set up the art booth?", {L"Only if my name goes first.", L"Sure, I have a little time.", L"Of course. Let's make it ours."}, 2}},
    {{L"One more lap before practice ends?", {L"Why do you try so hard?", L"I'll cheer you on.", L"Let's run it together!"}, 2},
     {L"We lost the match. I'm so annoyed.", {L"You never gave up on your team.", L"You should blame your teammates.", L"There's always next time."}, 0},
     {L"I'm nervous about tomorrow's final.", {L"You'll probably lose.", L"I'll be there in the front row.", L"Good luck tomorrow."}, 1}},
    {{L"This corner is usually quiet. Do you read?", {L"Sometimes, when I have time.", L"Yes. What's your favorite story?", L"Books are a waste of time."}, 1},
     {L"I wrote something, but it's not ready to show.", {L"Let me read it right now.", L"I'm sure it's fine.", L"Only when you're ready. No rush."}, 2},
     {L"Everyone talks over me at club meetings.", {L"I want to hear what you think.", L"Then talk louder.", L"That sounds difficult."}, 0}},
    {{L"I'm trying a new song for the festival.", {L"Play it for me. I'm listening.", L"Can you play something popular?", L"Sounds fun."}, 0},
     {L"I missed a note. Did you hear it?", {L"Yes, that was embarrassing.", L"The feeling came through. Keep going.", L"Don't worry about it."}, 1},
     {L"What if nobody comes to my performance?", {L"Maybe cancel it.", L"I'm sure someone will come.", L"I'll be there before the first note."}, 2}},
};

// An 8x8 ordered dither grid shared by every binary shade in the break.
constexpr int kBayer8[8][8] = {
    { 0, 32,  8, 40,  2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44,  4, 36, 14, 46,  6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    { 3, 35, 11, 43,  1, 33,  9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47,  7, 39, 13, 45,  5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21},
};

constexpr int kWalkerW = 14;
constexpr int kWalkerH = 18;
constexpr int kWalkerHeadRows = 9;
constexpr int kWalkerBodyRows = 9;

// A small hooded pilgrim, 14 x 18 pixels, white on the black floor. Only the
// head changes with the facing; the body changes with the walk pose.
const wchar_t* const kWalkerHeadDown[kWalkerHeadRows] = {
    L".....####.....",
    L"...########...",
    L"..##########..",
    L".############.",
    L".############.",
    L".####.##.####.",
    L".############.",
    L".############.",
    L"..##########..",
};

const wchar_t* const kWalkerHeadSide[kWalkerHeadRows] = {
    L".....####.....",
    L"...########...",
    L"..##########..",
    L".############.",
    L".############.",
    L".###.########.",
    L".############.",
    L".############.",
    L"..##########..",
};

const wchar_t* const kWalkerHeadUp[kWalkerHeadRows] = {
    L".....####.....",
    L"...########...",
    L"..##########..",
    L".############.",
    L".############.",
    L".############.",
    L".############.",
    L".############.",
    L"..##########..",
};

const wchar_t* const kWalkerBodyStand[kWalkerBodyRows] = {
    L"....######....",
    L"...########...",
    L"##############",
    L"#.##########.#",
    L"#.##########.#",
    L"#.##########.#",
    L"..##########..",
    L"...###..###...",
    L"...##....##...",
};

const wchar_t* const kWalkerBodyStepA[kWalkerBodyRows] = {
    L"....######....",
    L"...########...",
    L"##############",
    L"#.##########.#",
    L"#.##########.#",
    L"#.##########.#",
    L"..##########..",
    L"...###..###...",
    L"...##.........",
};

const wchar_t* const kWalkerBodyStepB[kWalkerBodyRows] = {
    L"....######....",
    L"...########...",
    L"##############",
    L"#.##########.#",
    L"#.##########.#",
    L"#.##########.#",
    L"..##########..",
    L"...###..###...",
    L".........##...",
};

constexpr UINT_PTR kSplashTimer = 1;
constexpr UINT_PTR kFrameTimer = 3;
constexpr UINT kFrameMs = 40;
constexpr ULONGLONG kSplashRevealMs = 1700;
// A beat of quiet angel before the title tries to strike.
constexpr ULONGLONG kSplashTitleDelayMs = 700;
constexpr ULONGLONG kSplashHoldMs = 5900;

// The title comes on like a fluorescent tube that is not quite ready: it
// stutters between dark, half lit and fully lit before it finally stays on.
// Fed by the splash timer, so each step lasts at least two frames.
struct TitleFlash {
    int lit;     // percentage of the title's height that is lit
    int millis;  // how long this step lasts
};

constexpr TitleFlash kTitleFlashes[] = {
    {0, 70},   {55, 60},  {0, 80},   {70, 60},  {0, 70},   {85, 70},
    {0, 110},  {55, 60},  {95, 70},  {0, 120},  {70, 60},  {100, 90},
    {0, 70},   {100, 120}, {85, 60}, {100, 160},
};

constexpr int titleFlickerMs() {
    int total = 0;
    for (const TitleFlash& flash : kTitleFlashes) total += flash.millis;
    return total;
}

// How much of the title is lit, 0-100, at a given point of the flicker.
int litTitlePercent(ULONGLONG flickerElapsed) {
    int remaining = static_cast<int>(flickerElapsed);
    for (const TitleFlash& flash : kTitleFlashes) {
        if (remaining < flash.millis) return flash.lit;
        remaining -= flash.millis;
    }
    return 100;
}


struct DotArt {
    std::vector<std::pair<int, int>> dots;
    std::vector<std::pair<int, int>> order;
    int rows = 0;
    int cols = 0;
};

const DotArt& seraphimArt() {
    static const DotArt art = [] {
        DotArt result;

        // Braille cells are two dots wide and four dots tall. Bit 0 is the
        // top-left dot; bits 3-5 are the right column and 6-7 the bottom row.
        constexpr int kDotColumn[8] = {0, 0, 0, 1, 1, 1, 0, 1};
        constexpr int kDotRow[8] = {0, 1, 2, 0, 1, 2, 3, 3};

        std::vector<std::pair<int, int>> dots;
        std::size_t position = 0;
        int lineIndex = 0;
        while (position < anaise::kSeraphimArt.size()) {
            std::size_t end = anaise::kSeraphimArt.find('\n', position);
            if (end == std::string_view::npos) {
                end = anaise::kSeraphimArt.size();
            }
            std::string_view line =
                anaise::kSeraphimArt.substr(position, end - position);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

            int cellColumn = 0;
            for (std::size_t i = 0; i < line.size();) {
                const auto lead = static_cast<unsigned char>(line[i]);
                std::uint32_t codepoint = 0;
                int length = 0;
                if (lead < 0x80) {
                    codepoint = lead;
                    length = 1;
                } else if ((lead & 0xE0) == 0xC0) {
                    codepoint = lead & 0x1F;
                    length = 2;
                } else if ((lead & 0xF0) == 0xE0) {
                    codepoint = lead & 0x0F;
                    length = 3;
                } else if ((lead & 0xF8) == 0xF0) {
                    codepoint = lead & 0x07;
                    length = 4;
                } else {
                    ++i;
                    continue;
                }
                if (i + static_cast<std::size_t>(length) > line.size()) break;
                for (int k = 1; k < length; ++k) {
                    codepoint =
                        (codepoint << 6) |
                        (static_cast<unsigned char>(line[i + k]) & 0x3F);
                }
                i += static_cast<std::size_t>(length);

                if (codepoint >= 0x2800 && codepoint <= 0x28FF) {
                    const std::uint32_t pattern = codepoint - 0x2800;
                    for (int bit = 0; bit < 8; ++bit) {
                        if (!(pattern & (1u << bit))) continue;
                        dots.emplace_back(lineIndex * 4 + kDotRow[bit],
                                          cellColumn * 2 + kDotColumn[bit]);
                    }
                }
                ++cellColumn;
            }
            ++lineIndex;
            position = end + 1;
        }

        if (dots.empty()) return result;

        // Trim the empty border so the angel can use the full splash area.
        int minRow = dots.front().first;
        int maxRow = minRow;
        int minCol = dots.front().second;
        int maxCol = minCol;
        for (const auto& dot : dots) {
            minRow = std::min(minRow, dot.first);
            maxRow = std::max(maxRow, dot.first);
            minCol = std::min(minCol, dot.second);
            maxCol = std::max(maxCol, dot.second);
        }
        for (auto& dot : dots) {
            dot.first -= minRow;
            dot.second -= minCol;
        }
        result.rows = maxRow - minRow + 1;
        result.cols = maxCol - minCol + 1;
        result.dots = std::move(dots);
        result.order = result.dots;

        const double centerRow = (result.rows - 1) / 2.0;
        const double centerCol = (result.cols - 1) / 2.0;
        auto distance = [centerRow, centerCol](const auto& dot) {
            const double dr = dot.first - centerRow;
            const double dc = dot.second - centerCol;
            return dr * dr + dc * dc;
        };
        std::sort(result.order.begin(), result.order.end(),
                  [&distance](const auto& a, const auto& b) {
                      return distance(a) < distance(b);
                  });
        return result;
    }();
    return art;
}

using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);

GetDpiForWindowFn dpiForWindowFn() {
    static GetDpiForWindowFn fn = []() -> GetDpiForWindowFn {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) return nullptr;
        return reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(user32, "GetDpiForWindow"));
    }();
    return fn;
}

UINT windowDpi(HWND hwnd) {
    if (auto fn = dpiForWindowFn()) {
        UINT dpi = fn(hwnd);
        if (dpi) return dpi;
    }
    return 96;
}

UINT systemDpi() {
    HDC dc = GetDC(nullptr);
    if (!dc) return 96;
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : 96;
}

void enableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    auto fn = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (fn) {
        const HANDLE perMonitorV2 = reinterpret_cast<HANDLE>(
            static_cast<LONG_PTR>(-4));
        if (fn(perMonitorV2)) return;
    }
    // Windows 7, 8.1 and early Windows 10 have no per-monitor API; fall back
    // to system DPI awareness so the window is not bitmap stretched.
    using SetProcessDPIAwareFn = BOOL(WINAPI*)();
    auto legacy = reinterpret_cast<SetProcessDPIAwareFn>(
        GetProcAddress(user32, "SetProcessDPIAware"));
    if (legacy) legacy();
}

void applyMonochromeTitleBar(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto fn = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (fn) {
        BOOL dark = TRUE;
        if (FAILED(fn(hwnd, 20, &dark, sizeof(dark)))) {
            fn(hwnd, 19, &dark, sizeof(dark));
        }
        COLORREF caption = kBlack;
        fn(hwnd, 35, &caption, sizeof(caption));
        COLORREF text = kWhite;
        fn(hwnd, 36, &text, sizeof(text));
        COLORREF border = kWhite;
        fn(hwnd, 34, &border, sizeof(border));
    }
    FreeLibrary(dwm);
}

// Loads an embedded typeface from RCDATA, so the executable carries the font
// and needs nothing installed on the machine.
HANDLE loadEmbeddedFont(HINSTANCE instance, int resourceId) {
    HRSRC resource =
        FindResourceW(instance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) return nullptr;
    HGLOBAL loaded = LoadResource(instance, resource);
    if (!loaded) return nullptr;
    void* data = LockResource(loaded);
    const DWORD size = SizeofResource(instance, resource);
    if (!data || !size) return nullptr;
    DWORD fonts = 0;
    HANDLE handle = AddFontMemResourceEx(data, size, nullptr, &fonts);
    return fonts ? handle : nullptr;
}

// ---------------------------------------------------------------- headless
// Development screens can be rendered straight into an offscreen bitmap with no
// window at all, then written as a normal PNG. Automated checks and capture
// scripts use this so nothing flashes on the desktop or steals focus.

enum class Screen {
    Splash,
    Menu,
    Library,
    Paint,
    Browser,
    // The break is captured as three screens so each stage can be rendered
    // offscreen on its own. In the app they are phases of the library.
    Crash,
    Search,
    Corridor,
    GodsRitual,
    GodsArena,
    GodsTower,
    Snake,
    Tentacle,
};

const wchar_t* screenName(Screen screen) {
    switch (screen) {
        case Screen::Splash: return L"splash";
        case Screen::Menu: return L"menu";
        case Screen::Library: return L"library";
        case Screen::Paint: return L"paint";
        case Screen::Browser: return L"browser";
        case Screen::Crash: return L"crash";
        case Screen::Search: return L"search";
        case Screen::Corridor: return L"corridor";
        case Screen::GodsRitual: return L"gods-ritual";
        case Screen::GodsArena: return L"gods-arena";
        case Screen::GodsTower: return L"gods-tower";
        case Screen::Snake: return L"snake";
        case Screen::Tentacle: return L"tentacle";
    }
    return L"screen";
}

// The library game runs in four phases: the rooms, the crash that ends them,
// the search page that answers the crash, and the corridor it finds.
enum class GamePhase { Play, Crash, Search, Corridor };

std::uint32_t pngCrc32(const std::uint8_t* data, std::size_t size) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t value = i;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1) ? 0xEDB88320u ^ (value >> 1) : value >> 1;
            }
            result[i] = value;
        }
        return result;
    }();
    std::uint32_t value = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        value = table[(value ^ data[i]) & 0xFF] ^ (value >> 8);
    }
    return value ^ 0xFFFFFFFFu;
}

void appendBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void appendPngChunk(std::vector<std::uint8_t>& out, const char type[4],
                    const std::vector<std::uint8_t>& data) {
    appendBigEndian(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    appendBigEndian(out, pngCrc32(out.data() + start, data.size() + 4));
}

std::uint32_t pngAdler32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

// Truecolour PNG with stored (uncompressed) deflate blocks. That keeps the app
// free of image libraries while still producing a file every viewer opens.
bool writePng(const std::wstring& path, const std::uint32_t* pixels,
              int width, int height) {
    if (width <= 0 || height <= 0 || !pixels) return false;

    // One filter byte (none) plus three colour bytes per pixel, top row first.
    const std::size_t stride = 1 + 3 * static_cast<std::size_t>(width);
    std::vector<std::uint8_t> raw;
    raw.reserve(stride * static_cast<std::size_t>(height));
    for (int row = 0; row < height; ++row) {
        raw.push_back(0);
        const std::uint32_t* line =
            pixels + static_cast<std::size_t>(row) * width;
        for (int col = 0; col < width; ++col) {
            const std::uint32_t pixel = line[col];
            raw.push_back(static_cast<std::uint8_t>(pixel));
            raw.push_back(static_cast<std::uint8_t>(pixel >> 8));
            raw.push_back(static_cast<std::uint8_t>(pixel >> 16));
        }
    }

    std::vector<std::uint8_t> stream;
    stream.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    stream.push_back(0x78);
    stream.push_back(0x01);
    std::size_t offset = 0;
    do {
        const std::size_t chunk = std::min<std::size_t>(65535, raw.size() - offset);
        stream.push_back(offset + chunk == raw.size() ? 1 : 0);
        stream.push_back(static_cast<std::uint8_t>(chunk));
        stream.push_back(static_cast<std::uint8_t>(chunk >> 8));
        const std::uint16_t inverse = static_cast<std::uint16_t>(~chunk);
        stream.push_back(static_cast<std::uint8_t>(inverse));
        stream.push_back(static_cast<std::uint8_t>(inverse >> 8));
        stream.insert(stream.end(), raw.begin() + offset,
                      raw.begin() + offset + chunk);
        offset += chunk;
    } while (offset < raw.size());
    appendBigEndian(stream, pngAdler32(raw.data(), raw.size()));

    std::vector<std::uint8_t> file = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A,
                                      '\n'};
    std::vector<std::uint8_t> header;
    appendBigEndian(header, static_cast<std::uint32_t>(width));
    appendBigEndian(header, static_cast<std::uint32_t>(height));
    header.push_back(8);  // bit depth
    header.push_back(2);  // colour type: truecolour
    header.push_back(0);  // compression: deflate
    header.push_back(0);  // filter: adaptive
    header.push_back(0);  // interlace: none
    appendPngChunk(file, "IHDR", header);
    appendPngChunk(file, "IDAT", stream);
    appendPngChunk(file, "IEND", {});

    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    std::size_t written = 0;
    bool ok = true;
    while (written < file.size()) {
        DWORD count = 0;
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(file.size() - written, 1 << 20));
        if (!WriteFile(handle, file.data() + written, chunk, &count, nullptr) ||
            count != chunk) {
            ok = false;
            break;
        }
        written += count;
    }
    CloseHandle(handle);
    return ok;
}

bool ensureDirectory(const std::wstring& path) {
    if (path.empty()) return false;
    std::wstring partial;
    partial.reserve(path.size());
    for (const wchar_t ch : path) {
        partial.push_back(ch);
        if ((ch == L'\\' || ch == L'/') && partial.size() > 1 &&
            !(partial.size() == 3 && partial[1] == L':')) {
            CreateDirectoryW(partial.c_str(), nullptr);
        }
    }
    CreateDirectoryW(partial.c_str(), nullptr);
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

struct HeadlessOptions {
    bool headless = false;
    bool allScreens = true;
    Screen screen = Screen::Menu;
    std::wstring outDir = L"build\\headless";
    int width = 1080;
    int height = 740;
    UINT dpi = 96;
    ULONGLONG timeMs = 4200;
    // A frozen mid-step, so a capture can show the pilgrim's movement light.
    int walkDir = -1;
    double walkProgress = 0.5;
    // Which room of the game a library capture opens (0-based).
    int room = 0;
    // Which Veil page a browser capture opens (1-based on the command line).
    int browserPage = 0;
    int browserScroll = 0;
    int browserPanel = 0;
    bool checkBrowser = false;
    // Headless resource check: parse the embedded tracks without a device.
    bool checkAudio = false;
    bool checkWindows = false;
    bool checkGame = false;
    bool checkGods = false;
    bool checkSnake = false;
    bool checkTentacle = false;
    int snakeView = 0;
    int godsView = 0;
    int schoolView = 0;  // Explore, conversation, or festival ending.
    int schoolGirl = 0;
    int schoolExpression = 0;
};

std::vector<std::wstring> splitCommandLine(const wchar_t* commandLine) {
    std::vector<std::wstring> args;
    std::wstring current;
    bool quoted = false;
    for (const wchar_t* p = commandLine; p && *p; ++p) {
        const wchar_t ch = *p;
        if (ch == L'"') {
            quoted = !quoted;
            continue;
        }
        if (!quoted && (ch == L' ' || ch == L'\t')) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) args.push_back(current);
    return args;
}

bool parseNumber(const std::wstring& text, ULONGLONG& value) {
    if (text.empty()) return false;
    wchar_t* end = nullptr;
    const unsigned long long parsed = std::wcstoull(text.c_str(), &end, 10);
    if (!end || *end != L'\0') return false;
    value = static_cast<ULONGLONG>(parsed);
    return true;
}

bool parseHeadlessOptions(const wchar_t* commandLine, HeadlessOptions& options) {
    const std::vector<std::wstring> args = splitCommandLine(commandLine);
    // Ordinary launches ignore their arguments, as they always have; only
    // --headless puts the app into the strict offscreen mode.
    if (std::find(args.begin(), args.end(), L"--headless") == args.end()) {
        return true;
    }
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::wstring& arg = args[i];
        if (arg == L"--headless") {
            options.headless = true;
            continue;
        }
        if (arg == L"--out" && i + 1 < args.size()) {
            options.outDir = args[++i];
            continue;
        }
        if (arg == L"--screen" && i + 1 < args.size()) {
            const std::wstring value = args[++i];
            options.allScreens = value == L"all";
            if (value == L"splash") options.screen = Screen::Splash;
            else if (value == L"menu") options.screen = Screen::Menu;
            else if (value == L"library") options.screen = Screen::Library;
            else if (value == L"paint") options.screen = Screen::Paint;
            else if (value == L"browser") options.screen = Screen::Browser;
            else if (value == L"crash") options.screen = Screen::Crash;
            else if (value == L"search") options.screen = Screen::Search;
            else if (value == L"corridor") options.screen = Screen::Corridor;
            else if (value == L"gods-ritual") options.screen = Screen::GodsRitual;
            else if (value == L"gods-arena") options.screen = Screen::GodsArena;
            else if (value == L"gods-tower") options.screen = Screen::GodsTower;
            else if (value == L"snake") options.screen = Screen::Snake;
            else if (value == L"tentacle") options.screen = Screen::Tentacle;
            else if (!options.allScreens) return false;
            continue;
        }
        if (arg == L"--room" && i + 1 < args.size()) {
            ULONGLONG room = 0;
            if (!parseNumber(args[++i], room) || room >= kRoomCount) {
                return false;
            }
            options.room = static_cast<int>(room);
            continue;
        }
        if (arg == L"--page" && i + 1 < args.size()) {
            ULONGLONG page = 0;
            if (!parseNumber(args[++i], page) || page < 1 ||
                page > kVeilPageCount) {
                return false;
            }
            options.browserPage = static_cast<int>(page) - 1;
            continue;
        }
        if (arg == L"--size" && i + 1 < args.size()) {
            const std::wstring value = args[++i];
            const std::size_t separator = value.find(L'x');
            if (separator == std::wstring::npos) return false;
            ULONGLONG width = 0;
            ULONGLONG height = 0;
            if (!parseNumber(value.substr(0, separator), width) ||
                !parseNumber(value.substr(separator + 1), height)) {
                return false;
            }
            if (width < 64 || height < 64 || width > 8192 || height > 8192) {
                return false;
            }
            options.width = static_cast<int>(width);
            options.height = static_cast<int>(height);
            continue;
        }
        if (arg == L"--time" && i + 1 < args.size()) {
            if (!parseNumber(args[++i], options.timeMs) ||
                options.timeMs > 3600000) {
                return false;
            }
            continue;
        }
        if (arg == L"--dpi" && i + 1 < args.size()) {
            ULONGLONG dpi = 0;
            if (!parseNumber(args[++i], dpi) || dpi < 48 || dpi > 384) {
                return false;
            }
            options.dpi = static_cast<UINT>(dpi);
            continue;
        }
        if (arg == L"--walk" && i + 1 < args.size()) {
            const std::wstring value = args[++i];
            if (value == L"down") options.walkDir = kFaceDown;
            else if (value == L"left") options.walkDir = kFaceLeft;
            else if (value == L"right") options.walkDir = kFaceRight;
            else if (value == L"up") options.walkDir = kFaceUp;
            else return false;
            continue;
        }
        if (arg == L"--progress" && i + 1 < args.size()) {
            ULONGLONG percent = 0;
            if (!parseNumber(args[++i], percent) || percent > 100) return false;
            options.walkProgress = static_cast<double>(percent) / 100.0;
            continue;
        }
        if (arg == L"--browser-scroll" && i + 1 < args.size()) {
            ULONGLONG value = 0;
            if (!parseNumber(args[++i], value) || value > 320) return false;
            options.browserScroll = static_cast<int>(value);
            continue;
        }
        if (arg == L"--browser-panel" && i + 1 < args.size()) {
            const auto value = args[++i];
            if (value == L"directory") options.browserPanel = 1;
            else if (value == L"history") options.browserPanel = 2;
            else if (value == L"bookmarks") options.browserPanel = 3;
            else return false;
            continue;
        }
        if (arg == L"--check-browser") {
            options.checkBrowser = true;
            continue;
        }
        if (arg == L"--check-windows") {
            options.checkWindows = true;
            continue;
        }
        if (arg == L"--check-game") {
            options.checkGame = true;
            continue;
        }
        if (arg == L"--snake-view" && i+1<args.size()) {
            ULONGLONG value=0; if(!parseNumber(args[++i],value)||value>1) return false;
            options.snakeView=int(value); continue;
        }
        if (arg == L"--check-snake") { options.checkSnake = true; continue; }
        if (arg == L"--check-tentacle") { options.checkTentacle = true; continue; }
        if (arg == L"--check-gods") {
            options.checkGods = true;
            continue;
        }
        if (arg == L"--gods-view" && i+1<args.size()) {
            ULONGLONG view=0;
            if(!parseNumber(args[++i],view)||view>10)return false;
            options.godsView=static_cast<int>(view);
            continue;
        }
        if (arg == L"--school-view" && i + 1 < args.size()) {
            ULONGLONG value = 0;
            if (!parseNumber(args[++i], value) || value > 4) return false;
            options.schoolView = static_cast<int>(value);
            continue;
        }
        if (arg == L"--school-girl" && i + 1 < args.size()) {
            ULONGLONG value = 0;
            if (!parseNumber(args[++i], value) || value < 1 || value > 4) return false;
            options.schoolGirl = static_cast<int>(value) - 1;
            continue;
        }
        if (arg == L"--school-expression" && i + 1 < args.size()) {
            const auto value = args[++i];
            if (value == L"neutral") options.schoolExpression = 0;
            else if (value == L"happy") options.schoolExpression = 1;
            else if (value == L"shy") options.schoolExpression = 2;
            else if (value == L"hurt") options.schoolExpression = 3;
            else if (value == L"afraid") options.schoolExpression = 4;
            else if (value == L"dead") options.schoolExpression = 5;
            else return false;
            continue;
        }
        if (arg == L"--check-audio") {
            options.checkAudio = true;
            continue;
        }
        return false;
    }
    return true;
}

class App {
public:
    ~App() {
        releaseFonts();
        releaseWalkerSurface();
        releaseRoomSurface();
        releaseBreakSurface();
        releaseWorldSurface();
        if (embeddedFont_) {
            RemoveFontMemResourceEx(embeddedFont_);
            embeddedFont_ = nullptr;
        }
        if (blackletterFontResource_) {
            RemoveFontMemResourceEx(blackletterFontResource_);
            blackletterFontResource_ = nullptr;
        }
    }

    bool create(HINSTANCE instance) {
        instance_ = instance;
        embeddedFont_ = loadEmbeddedFont(instance, kTitleFontResource);
        blackletterFontResource_ =
            loadEmbeddedFont(instance, kBlackletterFontResource);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &App::windowProc;
        wc.hInstance = instance;
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
        wc.hIconSm = reinterpret_cast<HICON>(
            LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
                       GetSystemMetrics(SM_CXSMICON),
                       GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"AnaiseWindow";
        if (!RegisterClassExW(&wc)) return false;

        const UINT dpi = systemDpi();
        RECT work{};
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
            work = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                    GetSystemMetrics(SM_CYSCREEN)};
        }
        int windowWidth = MulDiv(1080, static_cast<int>(dpi), 96);
        int windowHeight = MulDiv(740, static_cast<int>(dpi), 96);
        const int workWidth = work.right - work.left;
        const int workHeight = work.bottom - work.top;
        windowWidth = std::min(windowWidth,
                               workWidth - MulDiv(80, static_cast<int>(dpi), 96));
        windowHeight = std::min(windowHeight,
                                workHeight - MulDiv(80, static_cast<int>(dpi), 96));
        windowWidth = std::max(windowWidth,
                               MulDiv(680, static_cast<int>(dpi), 96));
        windowHeight = std::max(windowHeight,
                                MulDiv(460, static_cast<int>(dpi), 96));
        const int windowX = work.left + (workWidth - windowWidth) / 2;
        const int windowY = work.top + (workHeight - windowHeight) / 2;

        hwnd_ = CreateWindowExW(
            0, wc.lpszClassName, L"Anaise",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, windowX, windowY,
            windowWidth, windowHeight, nullptr, nullptr, instance, this);
        if (!hwnd_) return false;

        dpi_ = windowDpi(hwnd_);
        applyMonochromeTitleBar(hwnd_);
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        if (hwnd_) SetFocus(hwnd_);

        splashStart_ = GetTickCount64();
        SetTimer(hwnd_, kSplashTimer, 33, nullptr);
        // The ring pattern turns behind every screen but the seraphim, so it
        // gets a timer of its own that outlives the splash.
        SetTimer(hwnd_, kFrameTimer, kFrameMs, nullptr);
        // Music is optional: a machine with no output device stays silent and
        // the rest of the app is unaffected.
        if (audio_.open(instance)) audio_.play(anaise::Music::Intro);
        return true;
    }

    // Offscreen rendering for development: no window is created, nothing is
    // shown and no timer runs. renderHeadlessPng() draws one screen frozen at
    // `timeMs` of its animation, so captures are repeatable.
    bool createHeadless(HINSTANCE instance, int width, int height, UINT dpi) {
        if (width <= 0 || height <= 0 || dpi == 0) return false;
        instance_ = instance;
        headless_ = true;
        headlessWidth_ = width;
        headlessHeight_ = height;
        dpi_ = dpi;
        embeddedFont_ = loadEmbeddedFont(instance, kTitleFontResource);
        blackletterFontResource_ =
            loadEmbeddedFont(instance, kBlackletterFontResource);
        return true;
    }

    bool renderHeadlessPng(Screen screen, ULONGLONG timeMs,
                           const std::wstring& path, int walkDir = -1,
                           double walkProgress = 0.5, int browserPage = 0,
                           int room = 0, int browserScroll = 0, int browserPanel = 0,
                           int schoolView = 0, int schoolGirl = 0, int schoolExpression = 0, int godsView = 0, int snakeView = 0) {
        if (!headless_) return false;
        gods_.quit();
        appWindows_ = {};
        windowOrder_ = {0, 1, 2, 3, 4};
        snake_.reset();
        tentacle_.reset();
        phase_ = GamePhase::Play;
        screen_ = Screen::Menu;
        if (screen == Screen::Splash) screen_ = screen;
        else if (screen != Screen::Menu) openScreen(screen);
        headlessNowMs_ = timeMs;
        if (screen == Screen::Snake && snakeView) snake_.preview(timeMs);
        if (screen == Screen::Tentacle) tentacle_.preview(timeMs);
        if (screen == Screen::GodsRitual || screen == Screen::GodsArena || screen == Screen::GodsTower) {
            screen_ = Screen::Menu;
            gods_.preview(screen == Screen::GodsRitual ? anaise::NewGods::Stage::Revelation :
                          screen == Screen::GodsArena ? anaise::NewGods::Stage::Arena :
                          anaise::NewGods::Stage::Tower, timeMs, godsView);
        }
        splashStart_ = 0;
        browserPage_ = std::clamp(browserPage, 0, kBrowserPages - 1);
        browserTabs_ = {BrowserTab{{browserPage_}, 0, 0}, BrowserTab{{1}, 0, 0}};
        browserActive_ = 0; browserPanel_ = 0; browserEditing_ = false;
        browserPanelScroll_ = 0;
        browserTabs_[0].scroll = px(std::min(browserScroll, 320));
        browserPanel_ = browserPanel;
        browserHover_ = -1;
        browserLoadStart_ = 0;
        browserReloadStart_ = 0;

        // Every capture starts from a fresh run, in the room it asked for.
        resetRunState();
        roomId_ = std::clamp(room, 0, kRoomCount - 1);
        walker_ = Walker{};
        walker_.tileX = walker_.toX = kRooms[roomId_].startX;
        walker_.tileY = walker_.toY = kRooms[roomId_].startY;
        walker_.dir = kFaceDown;
        lastDir_ = -1;
        if (screen == Screen::Crash) {
            phase_ = GamePhase::Crash;
            phaseStart_ = 0;
        } else if (screen == Screen::Search) {
            phase_ = GamePhase::Search;
            phaseStart_ = 0;
        } else if (screen == Screen::Corridor) {
            beginCorridor();
            phaseStart_ = 0;
            if (schoolView > 0) {
                camX_ = kSchoolGirls[schoolGirl].x;
                camY_ = kSchoolGirls[schoolGirl].y + 1.4; camAngle_ = -kPi / 2;
                schoolTalking_ = schoolGirl;
            }
            schoolExpressions_[schoolGirl] = static_cast<SchoolExpression>(schoolExpression);
            schoolReactionUntil_[schoolGirl] = timeMs + 6500;
            if (schoolView == 2) {
                schoolHearts_[schoolGirl] = 5; schoolDate_ = schoolGirl; schoolTalking_ = -1;
            }
            if (schoolView >= 3) {
                schoolTalking_ = -1;
                schoolHorror_.found = schoolHorror_.equipped = schoolHorror_.alarm = true;
                schoolHorror_.actors[schoolGirl].fleeing = true;
                if (schoolView == 4) {
                    auto& actor = schoolHorror_.actors[schoolGirl]; actor.health = 0;
                    actor.hitAt = timeMs > 500 ? timeMs - 500 : 0;
                    schoolHorror_.lastVictim = schoolGirl;
                    schoolHorror_.blood.push_back({actor.x, actor.y, timeMs > 500 ? timeMs - 500 : 0, 1337});
                    schoolHorror_.swingAt = timeMs > 120 ? timeMs - 120 : 0; schoolHorror_.swings = 1;
                }
                schoolNotice_ = schoolView == 4 ? L"The music keeps playing. Nobody answers."
                    : L"A scream tears through the hall. Footsteps scatter.";
            }
        }

        // A capture can freeze the pilgrim halfway through a step, with the
        // pose and animation clock the game loop would have by then.
        if (screen == Screen::Library && walkDir >= 0 && walkDir < 4) {
            walker_.tileX = 6;
            walker_.tileY = 4;
            walker_.dir = walkDir;
            walker_.toX = walker_.tileX + kDirX[walkDir];
            walker_.toY = walker_.tileY + kDirY[walkDir];
            walker_.moving = true;
            walker_.progress = std::clamp(walkProgress, 0.0, 1.0);
            walker_.animMs = walker_.progress * kStepMs;
            static constexpr int kPoses[4] = {0, 1, 2, 1};
            walker_.pose =
                kPoses[static_cast<int>(walker_.animMs / kPoseMs) % 4];
        } else {
            walker_.moving = false;
            walker_.progress = 0.0;
            walker_.pose = kStandPose;
        }

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = headlessWidth_;
        info.bmiHeader.biHeight = -headlessHeight_;  // top-down rows
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) return false;
        void* bits = nullptr;
        HBITMAP bitmap =
            CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap || !bits) {
            if (bitmap) DeleteObject(bitmap);
            DeleteDC(dc);
            return false;
        }
        HGDIOBJ previous = SelectObject(dc, bitmap);
        paintInto(dc, headlessWidth_, headlessHeight_);
        GdiFlush();
        const bool ok =
            writePng(path, static_cast<const std::uint32_t*>(bits),
                     headlessWidth_, headlessHeight_);
        SelectObject(dc, previous);
        DeleteObject(bitmap);
        DeleteDC(dc);
        return ok;
    }

    bool checkSnakeInteractions() {
        if (!headless_) return false;
        if (!snake_.loadFaces()) return false;
        menuIndex_ = 3; activateMenuItem();
        if (screen_ != Screen::Snake || !appWindows_[3].open || activeApp() != 3) return false;
        handleKey(VK_RETURN); handleKey(VK_UP);
        snake_.update(100, true); snake_.update(300, true);
        dismissApp(false);
        if (!appWindows_[3].minimized) return false;
        focusApp(3);
        const RECT chip = menuLayout(headlessWidth_, headlessHeight_).taskChip[3];
        if (menuHitAt((chip.left+chip.right)/2,(chip.top+chip.bottom)/2).index != 3) return false;
        dismissApp(true);
        return !appWindows_[3].open;
    }

    bool checkTentacleInteractions() {
        if (!headless_) return false;
        menuIndex_ = 4; activateMenuItem();
        if (screen_ != Screen::Tentacle || !appWindows_[4].open || activeApp() != 4) return false;
        handleKey(VK_SPACE);
        if (tentacle_.title()) return false;
        handleKey('1');
        dismissApp(false);
        if (!appWindows_[4].minimized) return false;
        focusApp(4);
        const RECT chip = menuLayout(headlessWidth_, headlessHeight_).taskChip[4];
        if (menuHitAt((chip.left + chip.right) / 2, (chip.top + chip.bottom) / 2).index != 4) return false;
        dismissApp(true);
        return !appWindows_[4].open;
    }

    bool checkGodsInteractions() {
        if (!headless_) return false;
        enterMenu(); ensureFonts();
        const RECT hit = godsTitleRect();
        const int x = (hit.left + hit.right) / 2, y = (hit.top + hit.bottom) / 2;
        if (!godsTitleHit(x,y) || godsTitleHit(0,0)) return false;
        // Route through the actual desktop handler, including wrong-button input.
        for (int i=0;i<20;++i) onMouseDown(false,x,y);
        if (gods_.active()) return false;
        for (int i=0;i<14;++i) { headlessNowMs_=100+i*300;onMouseDown(true,x,y);onMouseUp(); }
        if (gods_.active()) return false;
        headlessNowMs_=4400;onMouseDown(true,x,y);onMouseUp();
        if (gods_.stage()!=anaise::NewGods::Stage::Rupture) return false;
        gods_.quit();
        openScreen(Screen::Browser);
        // An overlapping window must consume clicks instead of invoking the ritual.
        appWindows_[2].maximized=true;
        if (godsTitleHit(x,y)) return false;
        enterMenu();gods_.quit();
        return true;
    }

    bool checkWindowInteractions() {
        if (!headless_) return false;
        enterMenu();
        for (const auto& app : appWindows_) if (app.open) return false;
        auto click = [&](RECT r) {
            onMouseDown(true, (r.left + r.right) / 2, (r.top + r.bottom) / 2);
            onMouseUp();
        };
        RECT icon = menuLayout(headlessWidth_, headlessHeight_).iconHit[0];
        int iconX = (icon.left + icon.right) / 2;
        int iconY = (icon.top + icon.bottom) / 2;
        onMouseDown(true, iconX, iconY);
        onMouseMove(iconX + px(200), iconY + px(10));
        onMouseUp();
        RECT movedIcon = menuLayout(headlessWidth_, headlessHeight_).iconHit[0];
        if (movedIcon.left != icon.left + px(200) ||
            movedIcon.top != icon.top + px(10) || appWindows_[0].open ||
            menuHitAt(iconX + px(200), iconY + px(10)).index != 0) return false;
        // Losing capture must cancel a press instead of launching its app.
        RECT painterIcon = menuLayout(headlessWidth_, headlessHeight_).iconHit[1];
        onMouseDown(true, (painterIcon.left + painterIcon.right) / 2,
                          (painterIcon.top + painterIcon.bottom) / 2);
        handleMessage(WM_CAPTURECHANGED, 0, 0);
        onMouseUp();
        if (appWindows_[1].open) return false;
        // Launch from the same desktop icon and taskbar paths as live input.
        click(menuLayout(headlessWidth_, headlessHeight_).iconHit[1]);
        if (activeApp() != 1 || !appWindows_[1].open) return false;
        RECT body = appWindowLayout(1).windowBody;
        RECT client = contentClient(1);
        GridLayout grid = paintLayout(client.right, client.bottom);
        int x = body.left + MulDiv(grid.left + grid.cell / 2, body.right - body.left, client.right);
        int y = body.top + MulDiv(grid.top + grid.cell / 2, body.bottom - body.top, client.bottom);
        onMouseDown(true, x, y);
        onMouseUp();
        if (!canvas_[0][0]) return false;
        focusApp(0);
        focusApp(2);
        if (windowOrder_.back() != 2 || !appWindows_[0].open || !appWindows_[1].open) return false;
        RECT before = appWindowLayout(2).window;
        RECT title = appWindowLayout(2).windowTitle;
        x = title.left + px(40); y = (title.top + title.bottom) / 2;
        onMouseDown(true, x, y);
        onMouseMove(x - px(20), y - px(10));
        onMouseUp();
        RECT after = appWindowLayout(2).window;
        if (after.left != before.left - px(20) || after.top != before.top - px(10)) return false;
        click(appWindowLayout(2).windowMax);
        if (!appWindows_[2].maximized) return false;
        click(appWindowLayout(2).windowMax);
        if (appWindows_[2].maximized || appWindowLayout(2).window.left != after.left) return false;
        click(appWindowLayout(2).windowMin);
        if (!appWindows_[2].minimized || activeApp() != 0) return false;
        click(menuLayout(headlessWidth_, headlessHeight_).taskChip[2]);
        if (appWindows_[2].minimized || activeApp() != 2) return false;
        browserLoadStart_ = 0;
        body = appWindowLayout(2).windowBody;
        client = contentClient(2);
        RECT tab = browserLayout(client.right, client.bottom).tabs[1];
        x = body.left + MulDiv((tab.left + tab.right) / 2, body.right - body.left, client.right);
        y = body.top + MulDiv((tab.top + tab.bottom) / 2, body.bottom - body.top, client.bottom);
        onMouseDown(true, x, y); onMouseUp();
        if (browserPage_ != 1) return false;
        click(appWindowLayout(2).windowClose);
        if (appWindows_[2].open || activeApp() != 0) return false;
        click(appWindowLayout(0).windowClose);
        if (activeApp() != 1 || !canvas_[0][0]) return false;
        click(appWindowLayout(1).windowClose);
        if (screen_ != Screen::Menu || appWindows_[0].open || appWindows_[1].open) return false;
        focusApp(1);
        headlessWidth_ = 680;
        headlessHeight_ = 460;
        RECT resized = appWindowLayout(1).window;
        RECT desk = menuLayout(headlessWidth_, headlessHeight_).desktop;
        dismissApp(true);
        RECT movingIcon = menuLayout(headlessWidth_, headlessHeight_).iconHit[0];
        onMouseDown(true, (movingIcon.left + movingIcon.right) / 2,
                          (movingIcon.top + movingIcon.bottom) / 2);
        onMouseMove(10000, 10000);
        onMouseUp();
        RECT clampedIcon = menuLayout(headlessWidth_, headlessHeight_).iconHit[0];
        if (clampedIcon.right > desk.right || clampedIcon.bottom > desk.bottom ||
            appWindows_[0].open) return false;
        return resized.left >= desk.left && resized.top >= desk.top &&
               resized.right <= desk.right && resized.bottom <= desk.bottom;
    }

    // Offscreen regression for the four rooms: each lock holds until its
    // puzzle is answered, and each door lands where it says it will. The
    // walker is moved a whole tile at a time, as a finished step would move it.
    bool checkBrowserInteractions() {
        browserTabs_ = {BrowserTab{}}; browserActive_ = 0; browserPage_ = 0;
        browserGo(3); browserGo(8); browserHistory(-1);
        if (browserPage_ != 3) return false;
        browserHistory(1);
        if (browserPage_ != 8) return false;
        browserHistory(-1); browserGo(2); browserHistory(1);
        if (browserPage_ != 2 || browserTabs_[0].history.size() != 3) return false;
        browserScroll(px(140)); browserNewTab(); browserGo(10);
        browserSelectTab(0);
        if (browserPage_ != 2 || browserTabs_[0].scroll != px(140)) return false;
        browserSelectTab(1); browserHistory(-1);
        if (browserPage_ != 0) return false;
        browserAddress_ = L"nightmare.veil/registry"; browserSubmitAddress();
        if (browserPage_ != 10) return false;
        browserAddress_ = L"dragon"; browserSubmitAddress();
        if (browserPanel_ != 1 || browserRows().empty()) return false;
        browserAddress_ = L"unknown.invalid"; browserSubmitAddress();
        if (!browserRows().empty() || browserPanel_ != 1) return false;
        browserEditAddress(); browserCharacter(L'x'); browserCharacter(L'y'); browserCharacter(8);
        if (browserAddress_ != L"x") return false;
        browserEditing_ = false; browserQuery_.clear(); browserPanel_ = 3;
        browserBookmarks_[10] = true;
        if (browserRows() != std::vector<int>{10}) return false;
        browserCloseTab(0);
        if (browserActive_ != 0 || browserTabs_.size() != 1 || browserPage_ != 10) return false;
        browserCloseTab(0);
        if (browserPage_ != 0 || browserTabs_.size() != 1) return false;
        for (int i = 0; i < 8; ++i) browserNewTab();
        if (browserTabs_.size() != 6) return false;
        browserGo(4); browserGo(5); browserPanel_ = 2;
        if (browserRows().front() != 5) return false;
        for (int resource = 301; resource <= 306; ++resource) {
            HBITMAP bitmap = LoadBitmapW(instance_, MAKEINTRESOURCEW(resource));
            if (!bitmap) return false;
            BITMAP details{}; GetObjectW(bitmap, sizeof(details), &details); DeleteObject(bitmap);
            if (details.bmWidth != (resource <= 302 ? 640 : 256) ||
                details.bmHeight != (resource <= 302 ? 360 : 320)) return false;
        }
        for (int i = 0; i < 4; ++i)
            if (browserFaceResource(i) != 303 + i) return false;
        // Exercise the actual moving lens against a tiny offscreen surface:
        // the same time is identical; advancing time moves the glass.
        HDC reference = GetDC(nullptr);
        HDC lensDc = CreateCompatibleDC(reference);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = 128; info.bmiHeader.biHeight = -160;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* pixels = nullptr;
        HBITMAP lensBitmap = CreateDIBSection(reference, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        ReleaseDC(nullptr, reference);
        if (!lensDc || !lensBitmap || !pixels) {
            if (lensDc) DeleteDC(lensDc);
            if (lensBitmap) DeleteObject(lensBitmap);
            return false;
        }
        const auto oldBitmap = SelectObject(lensDc, lensBitmap);
        auto snapshot = [&](ULONGLONG time) {
            headlessNowMs_ = time;
            fillColor(lensDc, RECT{0, 0, 128, 160}, kBlack);
            drawBrowserWatcher(lensDc, 64, 80, 72, 0);
            GdiFlush();
            const auto* bytes = static_cast<const std::uint8_t*>(pixels);
            return std::vector<std::uint8_t>(bytes, bytes + 128 * 160 * 4);
        };
        const auto first = snapshot(1500);
        const auto repeat = snapshot(1500);
        const auto moved = snapshot(3200);
        SelectObject(lensDc, oldBitmap); DeleteDC(lensDc); DeleteObject(lensBitmap);
        return first == repeat && first != moved;
    }

    bool checkGameInteractions() {
        if (!headless_) return false;
        if (!ensureSchoolFaces()) return false;
        if (!anaise::SchoolHorror::selfTest()) return false;
        std::array<std::uint64_t, 4 * kSchoolFaceExpressions> portraitHashes{};
        for (int girl = 0; girl < 4; ++girl) for (int expression = 0; expression < kSchoolFaceExpressions; ++expression) {
            std::uint64_t hash = 14695981039346656037ull;
            for (int y = 0; y < kSchoolFaceSize; ++y) for (int x = 0; x < kSchoolFaceSize; ++x) {
                const auto pixel = schoolFacePixels_[girl][static_cast<std::size_t>(y) * kSchoolFaceSize * kSchoolFaceExpressions + expression * kSchoolFaceSize + x] & 0xFFFFFFu;
                if ((pixel & 255) != ((pixel >> 8) & 255) || (pixel & 255) != ((pixel >> 16) & 255)) return false;
                hash = (hash ^ pixel) * 1099511628211ull;
            }
            portraitHashes[girl * kSchoolFaceExpressions + expression] = hash;
        }
        std::sort(portraitHashes.begin(), portraitHashes.end());
        if (std::adjacent_find(portraitHashes.begin(), portraitHashes.end()) != portraitHashes.end()) return false;
        openScreen(Screen::Library);
        resetGame();
        if (roomId_ != 0 || key_ || walker_.tileX != 6 || walker_.tileY != 6) {
            return false;
        }
        auto arrive = [this](int x, int y) {
            walker_.tileX = walker_.toX = x;
            walker_.tileY = walker_.toY = y;
            walker_.moving = false;
            onArrive();
        };

        // Room I: the key on the floor unbars the north door.
        if (roomWalkable(6, 0)) return false;
        arrive(6, 3);
        if (!key_ || !keyTaken_ || !roomWalkable(6, 0)) return false;
        arrive(6, 0);
        if (roomId_ != 1 || walker_.tileX != 6 || walker_.tileY != 7) {
            return false;
        }

        // Room II: the levers only open the door on up, down, up.
        if (roomWalkable(5, 0)) return false;
        toggleLever(6, 2);
        if (leverSolved_) return false;
        toggleLever(6, 2);
        toggleLever(3, 2);
        if (leverSolved_) return false;
        toggleLever(9, 2);
        if (!leverSolved_ || !roomWalkable(5, 0)) return false;
        arrive(5, 0);
        if (roomId_ != 2 || walker_.tileX != 6 || walker_.tileY != 7) {
            return false;
        }

        // Room III: the plates must be stepped on I, II, III, and a wrong
        // plate makes the room forget.
        if (roomWalkable(4, 0)) return false;
        arrive(3, 4);
        if (plateStep_ != 1) return false;
        arrive(6, 6);
        if (plateStep_ != 0 || plateSolved_) return false;
        arrive(3, 4);
        arrive(9, 4);
        if (plateStep_ != 2 || plateSolved_) return false;
        arrive(6, 6);
        if (!plateSolved_ || !roomWalkable(4, 0)) return false;
        arrive(4, 0);
        if (roomId_ != 3 || walker_.tileX != 6 || walker_.tileY != 7) {
            return false;
        }

        // The 2D breach leads through the crash and search to the 3D school.
        arrive(6, 4);
        if (phase_ != GamePhase::Crash) return false;
        handleKey(VK_RETURN);
        if (phase_ != GamePhase::Search) return false;
        handleKey(VK_RETURN);
        if (phase_ != GamePhase::Corridor || schoolRizz_ != 0) return false;
        const auto started = headlessNowMs_;
        // Input cannot move the player during the loading card.
        moveCorridor(120, 1, 0);
        if (camX_ != 15.5) return false;
        headlessNowMs_ = started + kCorridorBootMs + 1;

        // Flood fill from the entrance proves every girl's room is reachable.
        std::array<bool, kCorridorCols * kCorridorRows> visited{};
        std::vector<POINT> pending{{15, 12}};
        visited[12 * kCorridorCols + 15] = true;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            for (int dir = 0; dir < 4; ++dir) {
                const int x = pending[i].x + kDirX[dir], y = pending[i].y + kDirY[dir];
                if (corridorSolid(x, y) || visited[y * kCorridorCols + x]) continue;
                visited[y * kCorridorCols + x] = true; pending.push_back(POINT{x, y});
            }
        }
        for (const auto& girl : kSchoolGirls)
            if (!visited[static_cast<int>(girl.y) * kCorridorCols + static_cast<int>(girl.x)]) return false;
        // Collision and turning share the exact live movement implementation.
        camX_ = 1.25; camY_ = 12.5; camAngle_ = kPi;
        for (int i = 0; i < 20; ++i) moveCorridor(120, 1, 0);
        if (camX_ < 1.22 || camX_ > 1.25) return false;
        moveCorridor(120, 0, 1);
        if (camAngle_ == kPi) return false;
        camX_ = 15.5; camY_ = 12.5; camAngle_ = 0;
        moveCorridor(120, 1, 0);
        if (camX_ <= 15.5) return false;
        interactSchool();
        if (schoolTalking_ != -1) return false;
        for (int girl = 0; girl < 4; ++girl) {
            camX_ = kSchoolGirls[girl].x; camY_ = kSchoolGirls[girl].y + 1.0;
            handleKey('E');
            if (schoolTalking_ != girl) return false;
            const double before = camX_;
            moveCorridor(120, 1, 1);
            if (camX_ != before) return false;
            handleKey(VK_BACK);
            if (schoolTalking_ != -1) return false;
            for (int round = 0; round < 3; ++round) {
                handleKey('E');
                handleKey('1' + kSchoolTalks[girl][round].best);
                if (schoolExpression(girl) != SchoolExpression::Happy) return false;
            }
            if (schoolHearts_[girl] != 5 || schoolRounds_[girl] != 3) return false;
            // A reply without a conversation must not award extra hearts.
            const int rizz = schoolRizz_;
            chooseSchoolReply(0);
            if (schoolRizz_ != rizz) return false;
        }
        handleKey('E'); handleKey('1');
        if (schoolDate_ != 3 || schoolExpression(3) != SchoolExpression::Shy) return false;
        handleKey('R');
        for (int heart : schoolHearts_) if (heart != 0) return false;
        if (schoolTalking_ != -1 || schoolDate_ != -1 || schoolRizz_ != 0) return false;
        headlessNowMs_ += kCorridorBootMs + 1;
        camX_ = 5.5; camY_ = 8.5;
        // Low affinity rejects an invitation; another conversation can recover.
        schoolRounds_[0] = 3;
        handleKey('E'); handleKey('1');
        if (schoolDate_ != -1 || schoolExpression(0) != SchoolExpression::Hurt) return false;
        handleKey('E'); handleKey('2');
        if (schoolRounds_[0] != 0) return false;
        handleKey('E'); handleKey('2');
        if (schoolHearts_[0] != 0 || schoolRizz_ != 0 || schoolExpression(0) != SchoolExpression::Hurt) return false;

        // The real school input handlers find/equip the knife and drive horror.
        beginCorridor(); headlessNowMs_ += kCorridorBootMs + 1;
        camX_ = anaise::SchoolHorror::knifeX; camY_ = anaise::SchoolHorror::knifeY;
        handleKey('E');
        if (!schoolHorror_.found || schoolHorror_.equipped) return false;
        handleKey('Q');
        camX_ = 5.5; camY_ = 8.5; camAngle_ = -kPi / 2;
        handleKey('F');
        if (schoolHorror_.actors[0].health != 2 || schoolExpression(0) != SchoolExpression::Afraid) return false;
        for (int hit = 0; hit < 2; ++hit) { headlessNowMs_ += 500; handleKey('F'); }
        if (schoolHorror_.actors[0].health != 0 || schoolExpression(0) != SchoolExpression::Dead || schoolTarget() == 0) return false;
        handleKey('R');
        if (schoolHorror_.found || schoolHorror_.equipped || schoolHorror_.alarm || !schoolHorror_.blood.empty()) return false;
        // Exercise all four escape routes against the real classroom map.
        for (auto& actor : schoolHorror_.actors) actor.fleeing = true;
        for (int step = 0; step < 250; ++step) {
            headlessNowMs_ += 120;
            schoolHorror_.tick(120, nowMs(), 15.5, 12.5, corridorSolid);
            for (const auto& actor : schoolHorror_.actors)
                if (corridorSolid(static_cast<int>(actor.x), static_cast<int>(actor.y))) return false;
        }
        for (const auto& actor : schoolHorror_.actors) if (!actor.escaped) return false;

        // And the way out resets the run to room I.
        endBreak();
        return phase_ == GamePhase::Play && roomId_ == 0 && !key_ &&
               !leverSolved_ && !plateSolved_ && plateStep_ == 0;
    }

private:
    static constexpr int kAppCount = 5;
    static constexpr int kMenuItems = kAppCount + 1;
    static constexpr int kBrowserPages = kVeilPageCount;
    static constexpr int kBrowserHitBack = kBrowserPages;
    static constexpr int kBrowserHitForward = kBrowserPages + 1;
    static constexpr int kBrowserHitReload = kBrowserPages + 2;
    static constexpr int kBrowserHitLink = kBrowserPages + 3;
    const wchar_t* const kMenuLabels[kMenuItems] = {
        L"BETHELAMENT",
        L"PAINTER",
        L"VEIL",
        L"SNAKE",
        L"TENTACLE",
        L"QUIT",
    };

    // The menu is a small monochrome operating system: every option is an app
    // icon on the desktop, with a finder window and a taskbar around it.
    const wchar_t* const kMenuTitles[kMenuItems] = {
        L"BETHELAMENT.EXE",
        L"PAINTER.EXE",
        L"VEIL.EXE",
        L"SNAKE.EXE",
        L"TENTACLE.EXE",
        L"SHUTDOWN.EXE",
    };
    // Veil reaches fifteen addresses. None of them resolve outside this file.
    struct VeilSite {
        const wchar_t* tab;
        const wchar_t* url;
        const wchar_t* host;
        const wchar_t* title;
        const wchar_t* link;
        int target;
        const wchar_t* marquee;
        const wchar_t* kicker;
        const wchar_t* heading;
        const wchar_t* subheading;
        const wchar_t* body;
        const wchar_t* caption;
        const wchar_t* facts;
        const wchar_t* footer;
        const wchar_t* badge;
        const wchar_t* const* art;
        int artCols;
        int artRows;
        int artSide;  // 0 none, 1 left, 2 right
    };

    static constexpr VeilSite kSites[kBrowserPages] = {
        // 0 -- the landing page
        {L"GAME", L"http://welcome-to-the-game.veil/",
         L"WELCOME-TO-THE-GAME.VEIL", L"WELCOME TO THE GAME",
         L"[ CONTINUE TO BAEL ]", 1,
         L"    WELCOME TO THE GAME    ///    YOU HAVE BEEN HERE BEFORE    ///    DO NOT REFRESH    ///    THE GAME IS PATIENT    ///    ",
         L"", L"", L"", L"", L"", L"", L"", L"", nullptr, 0, 0, 0},
        // 1 -- Bael
        {L"BAEL", L"http://bael.veil/one", L"BAEL.VEIL",
         L"BAEL - THE FIRST KING", L"[ SUMMON ]", 2,
         L"    SHRINE OF THE FIRST KING    ///    SIXTY-SIX LEGIONS    ///    HE HAS ALREADY SEEN YOU    ///    ",
         L"", L"", L"", L"", L"", L"", L"", L"", anaise::kBaelArt,
         anaise::kBaelArtCols, anaise::kBaelArtRows, 1},
        // 2 -- the order
        {L"LAMP", L"http://unlit-lamp.veil/join", L"UNLIT-LAMP.VEIL",
         L"THE ORDER OF THE UNLIT LAMP", L"[ APPLY IN WRITING ]", 14,
         L"    THE ORDER OF THE UNLIT LAMP    ///    MEETINGS 13TH NIGHT    ///    NO ADDRESS    ///    ",
         L"", L"", L"", L"", L"", L"", L"", L"", anaise::kSabbatArt,
         anaise::kSabbatArtCols, anaise::kSabbatArtRows, 2},
        // 3 -- the goat
        {L"GOAT", L"http://sabbatic-goat.veil/solve", L"SABBATIC-GOAT.VEIL",
         L"THE SABBATIC GOAT", L"[ RETURN TO THE ALTAR ]", 1,
         L"    THE SABBATIC GOAT    ///    SOLVE ET COAGULA    ///    THE TORCH CASTS NO SHADOW    ///    ",
         L"", L"", L"", L"", L"", L"", L"", L"", anaise::kBaphometArt,
         anaise::kBaphometArtCols, anaise::kBaphometArtRows, 1},
        // 4 -- Beelzebub
        {L"FLIES", L"http://beelzebub.veil/pest-control", L"BEELZEBUB.VEIL",
         L"BEELZEBUB - PEST CONTROL & WORSHIP", L"[ EXTERMINATE ]", 5,
         L"    24-HOUR SWARM SERVICE    ///    NO JOB TOO SMALL    ///    HE ANSWERS ON THE FIRST BUZZ    ///    ",
         L"HOST beelzebub.veil   //   PEST CONTROL   //   EST. 1666",
         L"BEELZEBUB", L"PEST CONTROL & WORSHIP",
         L"The Lord of the Flies is accepting new clients. We remove ants, wasps, and the sound of wings in an empty room. We do not remove what has already moved in.\nCall between 03:00 and 03:33. If a voice answers, hang up. If it keeps talking, you are already in the house.",
         L"FIG. 04 / THE LORD OF THE FLIES",
         L"SERVICE|SWARM\nRATE|A POUND OF YOU\nAVAILABILITY|ALWAYS",
         L"YELLOW PAGES 1966 // 1 (666) 555-0134", L"EXTERMINATE",
         anaise::kBeelzebubArt, anaise::kBeelzebubArtCols,
         anaise::kBeelzebubArtRows, 2},
        // 5 -- Asmodeus
        {L"ASMO", L"http://asmodeus.veil/love", L"ASMODEUS.VEIL",
         L"ASMODEUS DATING SERVICE", L"[ SWIPE RIGHT ]", 6,
         L"    ASMODEUS DATING SERVICE    ///    LOVE THAT OUTLIVES YOU    ///    NO REFUNDS AFTER DEATH    ///    ",
         L"HOST asmodeus.veil   //   TRUE LOVE   //   13,000 MATCHES",
         L"ASMODEUS", L"DATING SERVICE",
         L"He is a gentleman. He is patient. He will love you until you are gone, and then he will love what is left.\nFill in your name. Do not fill in your address. He already knows the way.",
         L"FIG. 05 / ASMODEUS, KING OF LUST",
         L"MATCHES|13,000\nSUCCESS|100%\nREFUNDS|NONE",
         L"PROUDLY SERVING THE LONELY SINCE 1666", L"SWIPE RIGHT",
         anaise::kAsmodeusArt, anaise::kAsmodeusArtCols,
         anaise::kAsmodeusArtRows, 1},
        // 6 -- Astaroth
        {L"STAR", L"http://astaroth.veil/library", L"ASTAROTH.VEIL",
         L"ASTAROTH'S SECRET LIBRARY", L"[ DOWNLOAD GRIMOIRE ]", 7,
         L"    ASTAROTH'S SECRET LIBRARY    ///    KNOWLEDGE IS A DOOR    ///    SOMETHING IS READING OVER YOUR SHOULDER    ///    ",
         L"HOST astaroth.veil   //   66,666 FILES   //   NO LOGIN",
         L"ASTAROTH", L"FREE GRIMOIRE LIBRARY",
         L"Download the complete works of Astaroth. Learn the names of things that should not be named. Learn the names of things that already know yours.\nAll files are free. All files are final. Do not print the pages. Do not read them aloud. Do not let them read you.",
         L"FIG. 06 / ASTAROTH, GRAND DUKE",
         L"FILES|66,666\nFORMAT|UNREADABLE\nPRICE|FREE",
         L"MIRROR 2: mirror.astaroth.veil (DO NOT USE)", L"DOWNLOAD",
         anaise::kAstarothArt, anaise::kAstarothArtCols,
         anaise::kAstarothArtRows, 2},
        // 7 -- Belphegor
        {L"SLOTH", L"http://belphegor.veil/confessional", L"BELPHEGOR.VEIL",
         L"BELPHEGOR'S CONFESSIONAL", L"[ CONFESS ]", 8,
         L"    BELPHEGOR'S CONFESSIONAL    ///    TELL US WHAT YOU DID    ///    THE CUSHION IS WARM    ///    ",
         L"HOST belphegor.veil   //   CONFIDENTIAL   //   OPEN ALL NIGHT",
         L"BELPHEGOR", L"CONFESSIONAL",
         L"Sit down. The chair has been waiting. You do not need to say anything. He already knows, and he is so very tired of hearing it.\nConfessions are free. Absolution is not. The difference is measured in hours you will never get back.",
         L"FIG. 07 / BELPHEGOR, DEMON OF SLOTH",
         L"WAIT TIME|ETERNITY\nSEATS|1\nEXIT|NONE",
         L"NO ONE HAS LEFT THIS PAGE. THAT IS NOT A TYPO.", L"CONFESS",
         anaise::kBelphegorArt, anaise::kBelphegorArtCols,
         anaise::kBelphegorArtRows, 1},
        // 8 -- Stolas
        {L"OWL", L"http://stolas.veil/observatory", L"STOLAS.VEIL",
         L"STOLAS OBSERVATORY", L"[ LOOK UP ]", 9,
         L"    STOLAS OBSERVATORY    ///    THE STARS ARE EYES    ///    THEY HAVE BEEN OPEN THE WHOLE TIME    ///    ",
         L"HOST stolas.veil   //   13 CONSTELLATIONS   //   LOOK UP",
         L"STOLAS", L"OBSERVATORY",
         L"Stolas knows the names of every star and the price of every wish. He is a patient bird. He has been watching you since before you had a name.\nTonight's forecast: clear. Tonight's advice: do not go outside. Tonight's sky: looking back.",
         L"FIG. 08 / STOLAS, 36TH SPIRIT",
         L"VISIBILITY|TOTAL\nHORIZON|CLOSING\nMOON|WRONG",
         L"OBSERVATORY LOG 13 // THE SKY MOVED FIRST", L"LOOK UP",
         anaise::kStolasArt, anaise::kStolasArtCols,
         anaise::kStolasArtRows, 2},
        // 9 -- Moloch
        {L"MOLOCH", L"http://moloch.veil/heating", L"MOLOCH.VEIL",
         L"MOLOCH HOME HEATING CO.", L"[ ADD FUEL ]", 10,
         L"    MOLOCH HOME HEATING    ///    WE KEEP THE FIRE ALIVE    ///    FIRST MONTH FREE    ///    ",
         L"HOST moloch.veil   //   WINTER SPECIAL   //   NO COLD CORNERS",
         L"MOLOCH", L"HOME HEATING CO.",
         L"Moloch has kept homes warm since the beginning of the world. His furnace has no off switch. His pipes run through rooms you have never seen.\nIf your radiators knock three times, do not answer. The heat is included. The cost is not.",
         L"FIG. 09 / MOLOCH, THE BURNING KING",
         L"FUEL|CONTINUOUS\nTEMPERATURE|BODY\nWARRANTY|VOID",
         L"CALL NOW // 1-666-HEAT // LINES OPEN UNTIL THE END", L"ADD FUEL",
         anaise::kMolochArt, anaise::kMolochArtCols,
         anaise::kMolochArtRows, 1},
        // 10 -- the registry
        {L"SLEEP", L"http://nightmare.veil/registry", L"NIGHTMARE.VEIL",
         L"THE NIGHTMARE REGISTRY", L"[ REGISTER YOUR SLEEP ]", 11,
         L"    THE NIGHTMARE REGISTRY    ///    REPORT YOUR DREAMS    ///    SOMETHING IS SITTING ON YOUR CHEST    ///    ",
         L"HOST nightmare.veil   //   REGISTRY   //   OPEN TO ALL SLEEPERS",
         L"THE NIGHTMARE REGISTRY", L"SLEEP IS A ROOM WITH NO DOOR",
         L"Every night, you visit the same room. It is not in your house. The registry has your name. The registry has always had your name.\nIf you wake up and cannot move, do not struggle. That is the registry updating your file.",
         L"FIG. 10 / THE NIGHTMARE, AFTER FUSELI",
         L"ENTRIES|YOU\nCHECKED IN|EVERY NIGHT\nCHECKED OUT|NEVER",
         L"LAST SYNC 03:33 // YOUR BODY WILL REMEMBER", L"SLEEP NOW",
         anaise::kNightmareArt, anaise::kNightmareArtCols,
         anaise::kNightmareArtRows, 2},
        // 11 -- the red dragon
        {L"DRAGON", L"http://red-dragon.veil/church", L"RED-DRAGON.VEIL",
         L"CHURCH OF THE RED DRAGON", L"[ JOIN THE CHOIR ]", 13,
         L"    CHURCH OF THE RED DRAGON    ///    THE SKY IS A LID    ///    THE SUN IS A HOLE IN IT    ///    ",
         L"HOST red-dragon.veil   //   13TH CHURCH   //   BRING NOTHING",
         L"CHURCH OF THE RED DRAGON", L"THE SKY IS A LID",
         L"We have seen what waits behind the sun. It has wings. It has been counting us. Its tail takes a third of the stars and the rest of your life.\nServices are held at dawn, underground. The choir sings backwards. The collection plate is empty because the dragon already has everything.",
         L"FIG. 11 / THE GREAT RED DRAGON, AFTER BLAKE",
         L"MEMBERS|13\nCOLLECTION|EVERYTHING\nDOOR|BEHIND YOU",
         L"THE SKY IS A LID // DO NOT LIFT IT", L"JOIN CHOIR",
         anaise::kRedDragonArt, anaise::kRedDragonArtCols,
         anaise::kRedDragonArtRows, 1},
        // 12 -- the guestbook
        {L"SABBAT", L"http://goya.veil/guestbook", L"GOYA.VEIL",
         L"THE SABBATH GUESTBOOK", L"[ SIGN THE GUESTBOOK ]", 14,
         L"    THE GUESTBOOK    ///    YOU WERE HERE    ///    WE SAW YOU    ///    ",
         L"HOST goya.veil   //   9,999 ENTRIES   //   SIGNED IN INK",
         L"THE SABBATH GUESTBOOK", L"EVERYONE SIGNED. NO ONE LEFT.",
         L"", L"FIG. 12 / WITCHES' SABBATH, AFTER GOYA", L"", L"",
         L"SIGN", anaise::kGoyaArt, anaise::kGoyaArtCols,
         anaise::kGoyaArtRows, 2},
        // 13 -- the morning star
        {L"LUCIFER", L"http://lucifer.veil/morning", L"LUCIFER.VEIL",
         L"MORNING STAR MINISTRIES", L"[ RENEW YOUR OATH ]", 12,
         L"    MORNING STAR MINISTRIES    ///    WE ONLY WANTED THE LIGHT    ///    NOW THE LIGHT WANTS US    ///    ",
         L"HOST lucifer.veil   //   DAWN SERVICE   //   03:33",
         L"MORNING STAR MINISTRIES", L"WE ONLY WANTED THE LIGHT",
         L"Once, he asked for one thing: to be seen. Now he is seen everywhere. He is in the chrome. He is in the static. He is in the back of every photograph you take alone.\nThere is no damnation here. Only the morning, arriving from the wrong direction, forever.",
         L"FIG. 13 / LUCIFER, MORNING STAR",
         L"SERVICE|DAWN\nDIRECTION|WRONG\nWELCOME|ALWAYS",
         L"MINISTRY OF THE UNLIT SUN // ALL ARE WELCOME // NONE LEAVE", L"LOOK EAST",
         anaise::kLuciferArt, anaise::kLuciferArtCols,
         anaise::kLuciferArtRows, 2},
        // 14 -- the missing page
        {L"404", L"http://welcome-to-the-game.veil/404",
         L"WELCOME-TO-THE-GAME.VEIL", L"404 - THE PAGE THAT WAS NEVER THERE",
         L"[ TAKE ME BACK ]", 0,
         L"    ERROR 404    ///    THE HOST ANSWERED IN YOUR VOICE    ///    RETRYING 99%    ///    ",
         L"", L"", L"", L"", L"", L"", L"", L"", nullptr, 0, 0, 0},
    };

    // 16 x 16 app glyphs, drawn as solid pixels onto each icon plate.
    static const wchar_t* const* appIconRows(int index) {
        static const wchar_t* const kBethelament[16] = {
            L"................",
            L"..############..",
            L"..#..........#..",
            L"..#.##.##.##.#..",
            L"..#.##.##.##.#..",
            L"..#.##.##.##.#..",
            L"..############..",
            L"..#..........#..",
            L"..#.##.###.#.#..",
            L"..#.##.###.#.#..",
            L"..#.##.###.#.#..",
            L"..############..",
            L"..#..........#..",
            L"..############..",
            L"...#........#...",
            L"................",
        };
        static const wchar_t* const kPainter[16] = {
            L"................",
            L"...........##...",
            L"..........####..",
            L".........####...",
            L"........####....",
            L".......####.....",
            L"......####......",
            L".....####.......",
            L".....###........",
            L"....###.........",
            L"...####.........",
            L"..#####.........",
            L"..####..........",
            L".####...........",
            L"................",
            L"................",
        };
        static const wchar_t* const kVeil[16] = {
            L"................",
            L".##############.",
            L".#............#.",
            L".#.##.#.......#.",
            L".##############.",
            L".#............#.",
            L".#....####....#.",
            L".#...#.#..#...#.",
            L".#..#..#...#..#.",
            L".#..########..#.",
            L".#..#..#...#..#.",
            L".#...#.#..#...#.",
            L".#....####....#.",
            L".#............#.",
            L".##############.",
            L"................",
        };
        static const wchar_t* const kSnake[16] = {
            L"................", L"................", L"..########......", L"..##....##......",
            L"..##....##......", L"..##....##......", L"..##....######..", L"..##........##..",
            L"..########..##..", L"........##..##..", L"........##..##..", L"........######..",
            L".........####...", L"..........##....", L"................", L"................",
        };
        static const wchar_t* const kTentacle[16] = {
            L"................", L".....######.....", L"....#......#....", L"...#.#....#.#...",
            L"...#........#...", L"...#.######.#...", L"...#.#.##.#.#...", L"....#......#....",
            L".....######.....", L"....#.#..#.#....", L"...#..#..#..#...", L"..#..#....#..#..",
            L"..#..#....#..#..", L".#..#......#..#.", L".#..#......#..#.", L"................",
        };
        static const wchar_t* const kShutdown[16] = {
            L"................",
            L".......##.......",
            L".......##.......",
            L".......##.......",
            L"....##.##.##....",
            L"...##..##..##...",
            L"..##...##...##..",
            L"..##........##..",
            L"..##........##..",
            L"..##........##..",
            L"...##......##...",
            L"....##....##....",
            L"......####......",
            L"................",
            L"................",
            L"................",
        };
        switch (index) {
            case 0: return kBethelament;
            case 1: return kPainter;
            case 2: return kVeil;
            case 3: return kSnake;
            case 4: return kTentacle;
            default: return kShutdown;
        }
    }

    enum class MenuPart {
        None,
        Icon,
        Start,
        StartItem,
        TaskChip,
        WindowClose,
        WindowMax,
        WindowMin,
        Signal,
    };

    struct MenuHit {
        MenuPart part = MenuPart::None;
        int index = -1;
    };

    struct MenuLayout {
        RECT monitor;
        RECT screen;
        RECT desktop;
        RECT taskBar;
        RECT iconTile[kMenuItems];
        RECT iconLabel[kMenuItems];
        RECT iconHit[kMenuItems];
        RECT window;
        RECT windowTitle;
        RECT windowBody;
        // The shell's working buttons. They are stored here so painting and
        // hit testing use exactly the same rectangles.
        RECT startButton;
        RECT taskChip[kMenuItems];
        RECT windowMin;
        RECT windowMax;
        RECT windowClose;
        RECT signal;
        RECT startMenu;
        RECT startItem[kMenuItems];
    };

    struct AppWindow {
        bool open = false;
        bool minimized = false;
        bool maximized = false;
        RECT bounds{};
    };

    void invalidate() const {
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    }

    int windowAt(int x, int y) const {
        for (auto it = windowOrder_.rbegin(); it != windowOrder_.rend(); ++it) {
            if (!appWindows_[*it].open || appWindows_[*it].minimized) continue;
            RECT bounds = appWindowLayout(*it).window;
            if (PtInRect(&bounds, POINT{x, y})) return *it;
        }
        return -1;
    }

    RECT shellClient() const {
        RECT client{0, 0, headlessWidth_, headlessHeight_};
        if (!headless_) GetClientRect(hwnd_, &client);
        return client;
    }

    static Screen appScreen(int index) {
        return index == 0 ? Screen::Library :
               index == 1 ? Screen::Paint : index == 2 ? Screen::Browser :
               index == 3 ? Screen::Snake : Screen::Tentacle;
    }

    int activeApp() const {
        if (screen_ == Screen::Snake) return 3;
        if (screen_ == Screen::Tentacle) return 4;
        if (screen_ == Screen::Paint) return 1;
        if (screen_ == Screen::Browser) return 2;
        if (screen_ == Screen::Library || screen_ == Screen::Crash ||
            screen_ == Screen::Search || screen_ == Screen::Corridor) {
            return 0;
        }
        return -1;
    }

    MenuLayout appWindowLayout(int index) const {
        const RECT client = shellClient();
        MenuLayout layout = menuLayout(client.right, client.bottom);
        const int pad = px(2);
        const RECT desk = layout.desktop;
        const AppWindow& app = appWindows_[index];
        RECT bounds = app.bounds;
        if (bounds.right == 0) {
            const int w = std::max(1, std::min(px(800), int(desk.right - desk.left) - px(24)));
            const int h = std::max(1, std::min(px(560), int(desk.bottom - desk.top) - px(24)));
            const int x = std::min(int(desk.left) + px(140 + index * 18), int(desk.right) - w - px(8));
            const int y = std::min(int(desk.top) + px(12 + index * 18), int(desk.bottom) - h - px(8));
            bounds = RECT{x, y, x + w, y + h};
        }
        int w = std::min(int(bounds.right - bounds.left), int(desk.right - desk.left));
        int h = std::min(int(bounds.bottom - bounds.top), int(desk.bottom - desk.top));
        int x = std::clamp(int(bounds.left), int(desk.left), int(desk.right) - w);
        int y = std::clamp(int(bounds.top), int(desk.top), int(desk.bottom) - h);
        layout.window = app.maximized ? desk : RECT{x, y, x + w, y + h};
        layout.windowTitle = RECT{layout.window.left + pad, layout.window.top + pad,
                                 layout.window.right - pad, layout.window.top + px(26)};
        layout.windowBody = RECT{layout.window.left + pad, layout.windowTitle.bottom,
                                layout.window.right - pad, layout.window.bottom - pad};
        RECT* boxes[] = {&layout.windowClose, &layout.windowMax, &layout.windowMin};
        for (int i = 0; i < 3; ++i) {
            int left = layout.windowTitle.right - px(4) - (i + 1) * px(22);
            *boxes[i] = RECT{left, layout.windowTitle.top + px(3), left + px(18),
                             layout.windowTitle.bottom - px(3)};
        }
        return layout;
    }

    RECT contentClient(int index) const {
        RECT body = appWindowLayout(index).windowBody;
        return RECT{0, 0, std::max(px(680), int(body.right - body.left)),
                          std::max(px(460), int(body.bottom - body.top))};
    }

    bool contentPoint(int& x, int& y) const {
        int index = activeApp();
        if (index < 0) return false;
        RECT body = appWindowLayout(index).windowBody;
        if (!painting_ && (!PtInRect(&body, POINT{x, y}) || windowAt(x, y) != index)) return false;
        RECT client = contentClient(index);
        x = MulDiv(x - body.left, client.right, body.right - body.left);
        y = MulDiv(y - body.top, client.bottom, body.bottom - body.top);
        return true;
    }

    // Window to content coordinates without the inside test, for drags that
    // leave the Tentacle window while a selection box is open.
    void tentaclePoint(int& x, int& y) const {
        RECT body = appWindowLayout(4).windowBody;
        RECT client = contentClient(4);
        x = MulDiv(x - body.left, client.right, std::max<LONG>(1, body.right - body.left));
        y = MulDiv(y - body.top, client.bottom, std::max<LONG>(1, body.bottom - body.top));
    }

    void focusApp(int index) {
        auto it = std::find(windowOrder_.begin(), windowOrder_.end(), index);
        std::rotate(it, it + 1, windowOrder_.end());
        openScreen(appScreen(index));
    }

    void dismissApp(bool close) {
        int index = activeApp();
        if (index < 0) return;
        if (close && index == 3) audio_.stopSnakeVoice();
        if (close) appWindows_[index].open = false;
        else appWindows_[index].minimized = true;
        enterMenu();
        for (auto it = windowOrder_.rbegin(); it != windowOrder_.rend(); ++it) {
            if (appWindows_[*it].open && !appWindows_[*it].minimized) {
                openScreen(appScreen(*it));
                break;
            }
        }
    }

    struct GridLayout {
        int left = 0;
        int top = 0;
        int cell = 0;
    };

    struct BrowserTab {
        std::vector<int> history{0};
        int cursor = 0;
        int scroll = 0;
    };

    struct BrowserLayout {
        RECT window;
        RECT title;
        RECT tabs[kBrowserPages];
        int tabsBottom = 0;
        RECT back;
        RECT forward;
        RECT reload;
        RECT address;
        RECT newTab, directory, history, bookmarks, star;
        RECT content;
    };

    struct RoomLayout {
        int left = 0;
        int top = 0;
        int cell = 0;
    };

    struct Walker {
        int tileX = 6;
        int tileY = 4;
        int toX = 6;
        int toY = 4;
        int dir = kFaceDown;
        int pose = kStandPose;
        bool moving = false;
        double progress = 0.0;
        double animMs = 0.0;
    };

    // ---------------------------------------------------------------- window
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
        App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = static_cast<App*>(cs->lpCreateParams);
            app->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(app));
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
        if (!app) return DefWindowProcW(hwnd, message, wParam, lParam);
        return app->handleMessage(message, wParam, lParam);
    }

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_ERASEBKGND:
                return 1;

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC dc = BeginPaint(hwnd_, &ps);
                paint(dc);
                EndPaint(hwnd_, &ps);
                return 0;
            }

            // PrintWindow and other offscreen copies go through here, so a
            // capture can take the client area without focusing the window.
            case WM_PRINTCLIENT:
                paint(reinterpret_cast<HDC>(wParam));
                return 0;

            case WM_SIZE:
                invalidate();
                return 0;

            case WM_TIMER:
                if (wParam == kSplashTimer) {
                    tickSplash();
                    return 0;
                }
                if (wParam == kFrameTimer) {
                    tickFrame();
                    return 0;
                }
                if (wParam == kGameTimer) {
                    tickGame();
                    return 0;
                }
                return 0;

            case WM_KILLFOCUS:
                audio_.pauseSnakeVoice(true);
                snake_.update(nowMs(), false);
                gods_.pause();
                godsMouseGuard_ = false;
                stopWalker();
                return 0;

            case WM_DPICHANGED: {
                dpi_ = HIWORD(wParam) ? HIWORD(wParam) : LOWORD(wParam);
                RECT* suggested = reinterpret_cast<RECT*>(lParam);
                if (suggested) {
                    SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                invalidate();
                return 0;
            }

            case WM_GETMINMAXINFO: {
                auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
                int dpi = hwnd_ ? static_cast<int>(dpi_) : 96;
                info->ptMinTrackSize.x = MulDiv(680, dpi, 96);
                info->ptMinTrackSize.y = MulDiv(460, dpi, 96);
                return 0;
            }

            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTCLIENT) {
                    const wchar_t* cursor = IDC_ARROW;
                    POINT point{};
                    GetCursorPos(&point);
                    ScreenToClient(hwnd_, &point);
                    int x = point.x, y = point.y;
                    if (gods_.active() || godsTitleHit(x, y)) {
                        SetCursor(LoadCursorW(nullptr, gods_.active() ? IDC_CROSS : IDC_HAND));
                        return TRUE;
                    }
                    if (contentPoint(x, y) && screen_ == Screen::Paint) {
                        cursor = IDC_CROSS;
                    } else if (menuHot_ != MenuPart::None ||
                               (screen_ == Screen::Browser && browserHover_ >= 0)) {
                        cursor = IDC_HAND;
                    }
                    SetCursor(LoadCursorW(nullptr, cursor));
                    return TRUE;
                }
                break;

            case WM_MOUSEMOVE:
                onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;

            case WM_MOUSELEAVE:
                mouseTracking_ = false;
                if (hoverValid_ && screen_ == Screen::Paint) {
                    hoverValid_ = false;
                    invalidate();
                }
                if (menuHot_ != MenuPart::None) {
                    menuHot_ = MenuPart::None;
                    menuHotIndex_ = -1;
                    invalidate();
                }
                return 0;

            case WM_MOUSEWHEEL: {
                const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta == 0) return 0;
                if (screen_ == Screen::Paint && !painting_) {
                    paintBrush_ = std::clamp(
                        paintBrush_ + (delta > 0 ? 1 : -1), 1, 3);
                    invalidate();
                    return 0;
                }
                if (screen_ == Screen::Browser) {
                    browserScroll(delta > 0 ? -px(64) : px(64));
                    return 0;
                }
                return 0;
            }

            case WM_LBUTTONDOWN:
                onMouseDown(true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
                onMouseUp();
                return 0;

            case WM_RBUTTONDOWN:
                onMouseDown(false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;

            case WM_CAPTURECHANGED:
                pressedIcon_ = -1;
                iconDragged_ = false;
                draggingApp_ = -1;
                painting_ = false;
                lastPaintValid_ = false;
                return 0;

            case WM_CHAR:
                if (screen_ == Screen::Browser) browserCharacter(static_cast<wchar_t>(wParam));
                return 0;

            case WM_KEYDOWN:
                if (gods_.active() && wParam != VK_F11) {
                    if (!(lParam & (1 << 30))) {
                        if (wParam == 'M') audio_.toggleMuted();
                        else gods_.key(static_cast<int>(wParam));
                        audio_.effects(gods_.takeSounds());
                        if (!gods_.active()) { godsMouseGuard_ = false; enterMenu(); }
                        invalidate();
                    }
                    return 0;
                }
                if ((lParam & (1 << 30)) && screen_ == Screen::Library && phase_ == GamePhase::Corridor &&
                    (wParam == 'E' || wParam == 'R' || wParam == 'F' || wParam == 'Q' || wParam == VK_RETURN || wParam == VK_SPACE ||
                     wParam == VK_BACK || (wParam >= '1' && wParam <= '3'))) return 0;
                if (wParam == VK_F11 && !(lParam & (1 << 30))) {
                    setFullscreen(!fullscreen_);
                    return 0;
                }
                if (wParam == VK_ESCAPE && screen_ == Screen::Browser &&
                    (browserEditing_ || browserPanel_)) {
                    browserEditing_ = false; browserPanel_ = 0; invalidate(); return 0;
                }
                if (wParam == VK_ESCAPE) {
                    if (fullscreen_) {
                        setFullscreen(false);
                        return 0;
                    }
                    if (screen_ == Screen::Splash ||
                        screen_ == Screen::Library || screen_ == Screen::Paint ||
                        screen_ == Screen::Browser || screen_ == Screen::Snake ||
                        screen_ == Screen::Tentacle ||
                        screen_ == Screen::Crash || screen_ == Screen::Search ||
                        screen_ == Screen::Corridor) {
                        if (screen_ == Screen::Splash) {
                            enterMenu();
                        } else {
                            if (screen_ == Screen::Library &&
                                phase_ != GamePhase::Play && phase_ != GamePhase::Corridor) {
                                endBreak();
                            }
                            dismissApp(false);
                        }
                        return 0;
                    }
                    if (screen_ == Screen::Menu) {
                        if (startMenuVisible_) {
                            startMenuVisible_ = false;
                            invalidate();
                            return 0;
                        }
                        return 0;
                    }
                }
                if ((screen_ == Screen::Snake || screen_ == Screen::Tentacle) && (lParam & (1 << 30))) return 0;
                handleKey(static_cast<int>(wParam));
                return 0;

            case WM_SYSKEYDOWN:
                if (wParam == VK_RETURN &&
                    (GetKeyState(VK_MENU) & 0x8000)) {
                    setFullscreen(!fullscreen_);
                    return 0;
                }
                return DefWindowProcW(hwnd_, message, wParam, lParam);

            case WM_SYSCHAR:
                if (wParam == VK_RETURN) return 0;
                return DefWindowProcW(hwnd_, message, wParam, lParam);

            case WM_THEMECHANGED:
            case WM_DWMCOMPOSITIONCHANGED:
                applyMonochromeTitleBar(hwnd_);
                return 0;

            case WM_CLOSE:
                DestroyWindow(hwnd_);
                return 0;

            case WM_DESTROY:
                KillTimer(hwnd_, kSplashTimer);
                KillTimer(hwnd_, kFrameTimer);
                KillTimer(hwnd_, kGameTimer);
                releaseFonts();
                releaseWalkerSurface();
                releaseRoomSurface();
                releaseBreakSurface();
                releaseWorldSurface();
                audio_.close();
                if (embeddedFont_) {
                    RemoveFontMemResourceEx(embeddedFont_);
                    embeddedFont_ = nullptr;
                }
                if (blackletterFontResource_) {
                    RemoveFontMemResourceEx(blackletterFontResource_);
                    blackletterFontResource_ = nullptr;
                }
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    void tickSplash() {
        if (screen_ != Screen::Splash) return;
        if (GetTickCount64() - splashStart_ >= kSplashHoldMs) {
            enterMenu();
        } else {
            invalidate();
        }
    }

    void tickFrame() {
        // The splash carries its own timer and no ring pattern; this one only
        // keeps the pattern turning behind the working screens.
        if (screen_ == Screen::Splash || !hwnd_) return;
        const bool snakeFocused = screen_ == Screen::Snake && !gods_.active() &&
                                  !IsIconic(hwnd_) && GetForegroundWindow() == hwnd_;
        snake_.update(nowMs(), snakeFocused);
        audio_.pauseSnakeVoice(!snakeFocused || snake_.paused());
        const int groan = snake_.takeGroan();
        if (groan >= 0) audio_.snakeGroan(groan);
        if (snake_.takeJam()) audio_.effects(4u);
        if (screen_ == Screen::Snake) audio_.play(snake_.cinematic() ? anaise::Music::None : anaise::Music::Menu);
        const bool tentacleFocused = screen_ == Screen::Tentacle && !gods_.active() &&
                                     !IsIconic(hwnd_) && GetForegroundWindow() == hwnd_;
        tentacle_.update(nowMs(), tentacleFocused);
        if (screen_ == Screen::Tentacle) {
            audio_.tentacle(tentacle_.takeSounds());
            audio_.play(tentacle_.fighting() ? anaise::Music::Room : anaise::Music::Menu);
        }
        if (IsIconic(hwnd_)) return;
        if (gods_.active()) {
            auto held = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
            anaise::NewGods::Input input;
            if (GetForegroundWindow() == hwnd_) {
                input.forward = double(held('W') || held(VK_UP)) - double(held('S') || held(VK_DOWN));
                input.strafe = double(held('D')) - double(held('A'));
                input.turn = double(held(VK_RIGHT)) - double(held(VK_LEFT));
                input.guard = held('K') || (godsMouseGuard_ && held(VK_RBUTTON));
            }
            gods_.update(nowMs(), input);
            audio_.effects(gods_.takeSounds());
            audio_.play(anaise::Music::Hymn);
        }
        invalidate();
    }

    void enterMenu() {
        if (hwnd_) KillTimer(hwnd_, kSplashTimer);
        if (hwnd_) KillTimer(hwnd_, kGameTimer);
        stopWalker();
        if (painting_) {
            painting_ = false;
            lastPaintValid_ = false;
            if (hwnd_ && GetCapture() == hwnd_) ReleaseCapture();
        }
        snake_.update(nowMs(), false);
        audio_.pauseSnakeVoice(true);
        screen_ = Screen::Menu;
        if (hwnd_) SetWindowTextW(hwnd_, L"Anaise");
        menuHot_ = MenuPart::None;
        menuHotIndex_ = -1;
        startMenuVisible_ = false;
        audio_.play(anaise::Music::Menu);
        invalidate();
    }

    void openScreen(Screen screen) {
        const int index = screen == Screen::Paint ? 1
                          : screen == Screen::Snake ? 3
                          : screen == Screen::Tentacle ? 4
                          : screen == Screen::Browser ? 2
                                                      : 0;
        const bool newlyOpened = !appWindows_[index].open;
        appWindows_[index].open = true;
        appWindows_[index].minimized = false;
        hoverValid_ = false;
        stopWalker();
        if (painting_) {
            painting_ = false;
            lastPaintValid_ = false;
            if (hwnd_ && GetCapture() == hwnd_) ReleaseCapture();
        }
        const bool playing = screen == Screen::Library &&
                             phase_ == GamePhase::Play;
        if (playing) {
            lastDir_ = -1;
            gameTickLast_ = nowMs();
            if (hwnd_) SetTimer(hwnd_, kGameTimer, kGameTickMs, nullptr);
        } else if (screen == Screen::Library) {
            if (hwnd_) SetTimer(hwnd_, kGameTimer, kGameTickMs, nullptr);
        } else {
            if (hwnd_) KillTimer(hwnd_, kGameTimer);
        }
        if (screen == Screen::Browser && newlyOpened) {
            browserHover_ = -1;
            browserLoadStart_ = nowMs();
        }
        if (screen == Screen::Snake) {
            snake_.loadFaces();
            if (newlyOpened) { audio_.stopSnakeVoice(); snake_.reset(); }
        }
        if (screen != Screen::Snake) snake_.update(nowMs(), false);
        if (screen == Screen::Tentacle && newlyOpened) tentacle_.reset();
        if (screen != Screen::Tentacle) tentacle_.mouseUp();
        screen_ = screen;
        if (hwnd_) {
            switch (screen) {
                case Screen::Library:
                    SetWindowTextW(hwnd_, L"Anaise - Bethelament");
                    break;
                case Screen::Snake:
                    SetWindowTextW(hwnd_, L"Anaise - Snake");
                    break;
                case Screen::Tentacle:
                    SetWindowTextW(hwnd_, L"Anaise - Tentacle");
                    break;
                case Screen::Paint:
                    SetWindowTextW(hwnd_, L"Anaise - Painter");
                    break;
                case Screen::Browser:
                    SetWindowTextW(hwnd_, L"Anaise - Veil");
                    break;
                case Screen::Crash:
                    SetWindowTextW(hwnd_, L"Bethelament.exe (Not Responding)");
                    break;
                case Screen::Search:
                    SetWindowTextW(hwnd_, L"Veil Search (Not Responding)");
                    break;
                case Screen::Corridor:
                    SetWindowTextW(hwnd_, L"Anaise - SECTOR 03");
                    break;
                default:
                    SetWindowTextW(hwnd_, L"Anaise");
                    break;
            }
        }
        startMenuVisible_ = false;
        menuHot_ = MenuPart::None;
        menuHotIndex_ = -1;
        audio_.play(screen == Screen::Library && phase_ == GamePhase::Corridor
                        ? (schoolHorror_.alarm ? anaise::Music::SchoolHorror : anaise::Music::School)
                        : screen == Screen::Library || screen == Screen::Paint ||
                            screen == Screen::Crash ||
                            screen == Screen::Search ||
                            screen == Screen::Corridor
                        ? anaise::Music::Room
                        : anaise::Music::Menu);
        invalidate();
    }

    void activateMenuItem() {
        switch (menuIndex_) {
            case 0:
                focusApp(0);
                break;
            case 1:
                focusApp(1);
                break;
            case 2:
                focusApp(2);
                break;
            case 3:
                focusApp(3);
                break;
            case 4:
                focusApp(4);
                break;
            default:
                DestroyWindow(hwnd_);
                break;
        }
    }

    // ---------------------------------------------------------------- browser
    void browserGo(int page) {
        page = std::clamp(page, 0, kBrowserPages - 1);
        auto& tab = browserTabs_[browserActive_];
        if (tab.history[tab.cursor] != page) {
            tab.history.resize(tab.cursor + 1);
            tab.history.push_back(page);
            if (tab.history.size() > 128) tab.history.erase(tab.history.begin());
            tab.cursor = static_cast<int>(tab.history.size()) - 1;
        }
        tab.scroll = 0;
        browserPage_ = page;
        browserVisits_.push_back(page);
        if (browserVisits_.size() > 128) browserVisits_.erase(browserVisits_.begin());
        browserPanel_ = 0;
        browserEditing_ = false;
        browserQuery_.clear();
        browserPanelScroll_ = 0;
        browserHover_ = -1;
        browserLoadStart_ = nowMs();
        invalidate();
    }

    void browserHistory(int delta) {
        auto& tab = browserTabs_[browserActive_];
        tab.cursor = std::clamp(tab.cursor + delta, 0, static_cast<int>(tab.history.size()) - 1);
        browserPage_ = tab.history[tab.cursor];
        tab.scroll = 0;
        browserPanel_ = 0;
        browserEditing_ = false;
        browserLoadStart_ = nowMs();
        invalidate();
    }

    void browserSelectTab(int index) {
        if (index < 0 || index >= static_cast<int>(browserTabs_.size())) return;
        browserActive_ = index;
        browserPanelScroll_ = 0;
        browserPage_ = browserTabs_[index].history[browserTabs_[index].cursor];
        browserPanel_ = 0;
        browserEditing_ = false;
        browserLoadStart_ = 0;
        invalidate();
    }

    void browserNewTab() {
        if (browserTabs_.size() >= 6) return;
        browserTabs_.push_back(BrowserTab{});
        browserSelectTab(static_cast<int>(browserTabs_.size()) - 1);
        browserPanel_ = 1;
    }

    void browserCloseTab(int index) {
        if (browserTabs_.size() == 1) {
            browserTabs_[0] = BrowserTab{};
            browserSelectTab(0);
            browserPanel_ = 1;
        } else {
            browserTabs_.erase(browserTabs_.begin() + index);
            browserSelectTab(browserActive_ > index ? browserActive_ - 1 :
                std::min(browserActive_, static_cast<int>(browserTabs_.size()) - 1));
        }
    }

    void browserEditAddress() {
        browserEditing_ = true;
        browserAddress_ = kSites[browserPage_].url;
        browserSelectAll_ = true;
        invalidate();
    }

    void browserPasteAddress() {
        if (!OpenClipboard(hwnd_)) return;
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if (handle) {
            const auto* text = static_cast<const wchar_t*>(GlobalLock(handle));
            if (text) {
                if (browserSelectAll_) browserAddress_.clear();
                for (int i = 0; text[i] && browserAddress_.size() < 240; ++i)
                    if (text[i] >= 32) browserAddress_ += text[i];
                GlobalUnlock(handle);
                browserSelectAll_ = false;
            }
        }
        CloseClipboard();
        invalidate();
    }

    static std::wstring browserLower(std::wstring value) {
        for (auto& c : value) c = static_cast<wchar_t>(std::towlower(c));
        return value;
    }

    std::vector<int> browserRows() const {
        if (browserPanel_ == 2) return std::vector<int>(browserVisits_.rbegin(), browserVisits_.rend());
        std::vector<int> rows;
        const auto query = browserLower(browserQuery_);
        for (int i = 0; i < kBrowserPages; ++i) {
            if (browserPanel_ == 3 && !browserBookmarks_[i]) continue;
            const auto hay = browserLower(std::wstring(kSites[i].url) + L" " + kSites[i].title + L" " + kSites[i].tab);
            if (query.empty() || hay.find(query) != std::wstring::npos) rows.push_back(i);
        }
        return rows;
    }

    void browserSubmitAddress() {
        browserEditing_ = false;
        browserPanelScroll_ = 0;
        const auto query = browserLower(browserAddress_);
        for (int i = 0; i < kBrowserPages; ++i) {
            std::wstring url = browserLower(kSites[i].url);
            if (query == url || query == url.substr(7)) { browserGo(i); return; }
        }
        browserQuery_ = browserAddress_;
        browserPanel_ = 1;
        browserTabs_[browserActive_].scroll = 0;
        invalidate();
    }

    void browserCharacter(wchar_t c) {
        if (!browserEditing_ || (GetKeyState(VK_CONTROL) & 0x8000)) return;
        if (c == 8) {
            if (browserSelectAll_) browserAddress_.clear();
            else if (!browserAddress_.empty()) browserAddress_.pop_back();
        } else if (c >= 32 && browserAddress_.size() < 240) {
            if (browserSelectAll_) browserAddress_.clear();
            browserAddress_ += c;
        }
        browserSelectAll_ = false;
        invalidate();
    }

    void browserScroll(int delta) {
        auto& scroll = browserPanel_ ? browserPanelScroll_ : browserTabs_[browserActive_].scroll;
        const RECT client = contentClient(2);
        const auto layout = browserLayout(client.right, client.bottom);
        const int maxScroll = browserPanel_ ? std::max(0, static_cast<int>(browserRows().size()) * px(30) -
            static_cast<int>(layout.content.bottom - layout.content.top) + px(66)) : px(320);
        scroll = std::clamp(scroll + delta, 0, maxScroll);
        invalidate();
    }

    void browserFollowLink() {
        int target = kSites[browserPage_].target;
        if (target < 0 || target >= kBrowserPages) {
            target = (browserPage_ + 1) % kBrowserPages;
        }
        browserGo(target);
    }

    void browserReload() {
        browserHover_ = -1;
        browserLoadStart_ = nowMs();
        browserReloadStart_ = nowMs();
        invalidate();
    }

    bool browserLoading() const {
        if (browserLoadStart_ == 0) return false;
        return nowMs() - browserLoadStart_ < 720;
    }

    void handleKey(int key) {
        // The signal bars are a small target; M mutes from any screen.
        if (key == 'M' && !(screen_ == Screen::Browser && browserEditing_)) {
            audio_.toggleMuted();
            invalidate();
            return;
        }
        if (screen_ == Screen::Splash) {
            enterMenu();
            return;
        }
        if (screen_ == Screen::Menu) {
            if (key == VK_UP || key == 'W') {
                menuIndex_ = (menuIndex_ + kMenuItems - 1) % kMenuItems;
                invalidate();
            } else if (key == VK_DOWN || key == 'S') {
                menuIndex_ = (menuIndex_ + 1) % kMenuItems;
                invalidate();
            } else if (key == VK_RETURN || key == VK_SPACE) {
                activateMenuItem();
            }
            return;
        }
        if (screen_ == Screen::Tentacle) {
            tentacle_.key(key);
            audio_.tentacle(tentacle_.takeSounds());
            invalidate(); return;
        }
        if (screen_ == Screen::Snake) {
            snake_.key(key);
            audio_.pauseSnakeVoice(snake_.paused());
            if (!snake_.cinematic()) audio_.stopSnakeVoice();
            invalidate(); return;
        }
        if (screen_ == Screen::Library && phase_ != GamePhase::Play) {
            if (phase_ == GamePhase::Corridor) {
                if (key == 'R') beginCorridor();
                else if (key == 'Q') { schoolHorror_.toggle(); schoolTalking_ = -1; invalidate(); }
                else if (key == 'F') attackSchool();
                else if (key == VK_BACK) { schoolTalking_ = -1; invalidate(); }
                else if (key >= '1' && key <= '3') chooseSchoolReply(key - '1');
                else if (key == VK_RETURN || key == VK_SPACE || key == 'E') {
                    if (schoolDate_ >= 0) endBreak();
                    else interactSchool();
                }
                return;
            }
            if (key == VK_RETURN || key == VK_SPACE || key == 'E') {
                if (phase_ == GamePhase::Crash) beginSearch();
                else if (phase_ == GamePhase::Search) beginCorridor();
            }
            return;
        }
        if (screen_ == Screen::Browser) {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl && key == 'L') browserEditAddress();
            else if (ctrl && key == 'T') browserNewTab();
            else if (ctrl && key == 'W') browserCloseTab(browserActive_);
            else if (ctrl && key == VK_TAB) browserSelectTab((browserActive_ + 1) % browserTabs_.size());
            else if (ctrl && key == 'H') { browserPanel_ = 2; browserPanelScroll_ = 0; browserQuery_.clear(); }
            else if (ctrl && key == 'B') { browserPanel_ = 3; browserPanelScroll_ = 0; browserQuery_.clear(); }
            else if (ctrl && key == 'D') browserBookmarks_[browserPage_] = !browserBookmarks_[browserPage_];
            else if (browserEditing_) {
                if (key == VK_RETURN) browserSubmitAddress();
                else if (ctrl && key == 'A') browserSelectAll_ = true;
                else if (ctrl && key == 'V') browserPasteAddress();
                else if (key == VK_DELETE) { browserAddress_.clear(); browserSelectAll_ = false; }
            } else if (key == VK_LEFT || key == VK_BACK) browserHistory(-1);
            else if (key == VK_RIGHT) browserHistory(1);
            else if (key == VK_F5 || (ctrl && key == 'R')) browserReload();
            else if (key == VK_HOME) { if (browserPanel_) browserPanelScroll_ = 0; else browserTabs_[browserActive_].scroll = 0; }
            else if (key == VK_END) browserScroll(px(100000));
            else if (key == VK_NEXT || key == VK_SPACE) browserScroll(px(180));
            else if (key == VK_PRIOR) browserScroll(-px(180));
            else if (key == VK_DOWN) browserScroll(px(40));
            else if (key == VK_UP) browserScroll(-px(40));
            else if (key == VK_RETURN) browserFollowLink();
            invalidate();
            return;
        }
        if (screen_ == Screen::Paint) {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (key == 'C') {
                clearCanvas();
            } else if (key == '1' || key == '2' || key == '3') {
                paintBrush_ = key - '0';
                invalidate();
            } else if (key == 'Z' && ctrl && shift) {
                redoCanvas();
            } else if (key == 'Z' && ctrl) {
                undoCanvas();
            } else if (key == 'Y' && ctrl) {
                redoCanvas();
            }
        } else if (screen_ == Screen::Library) {
            const int dir = directionForKey(key);
            if (dir >= 0) {
                lastDir_ = dir;
            } else if (key == 'E' || key == VK_SPACE || key == VK_RETURN) {
                const int col = walker_.tileX + kDirX[walker_.dir];
                const int row = walker_.tileY + kDirY[walker_.dir];
                if (roomTile(roomId_, col, row) == L'L') {
                    toggleLever(col, row);
                } else {
                    setNotice(L"Nothing here answers your hand.");
                    invalidate();
                }
            } else if (key == 'R') {
                resetGame();
            }
        }
    }

    static int directionForKey(int key) {
        switch (key) {
            case VK_DOWN:
            case 'S':
                return kFaceDown;
            case VK_LEFT:
            case 'A':
                return kFaceLeft;
            case VK_RIGHT:
            case 'D':
                return kFaceRight;
            case VK_UP:
            case 'W':
                return kFaceUp;
            default:
                return -1;
        }
    }

    // ------------------------------------------------------------ bethelament
    static char roomTile(int room, int col, int row) {
        if (room < 0 || room >= kRoomCount || col < 0 || col >= kRoomCols ||
            row < 0 || row >= kRoomRows) {
            return L'#';
        }
        return kRooms[room].rows[row][col];
    }

    static bool solidTile(char tile) {
        return tile == L'#' || tile == L'B' || tile == L'L' || tile == L'G' ||
               tile == L'C';
    }

    // The doorway standing on a tile, if any.
    static const RoomDoor* doorAt(int room, int col, int row) {
        if (room < 0 || room >= kRoomCount) return nullptr;
        for (const RoomDoor& door : kRoomDoors[room]) {
            if (door.x == col && door.y == row) return &door;
        }
        return nullptr;
    }

    bool doorUnlocked(const RoomDoor& door) const {
        switch (door.lock) {
            case kDoorKey: return key_;
            case kDoorLevers: return leverSolved_;
            case kDoorPlates: return plateSolved_;
            default: return true;
        }
    }

    bool roomWalkable(int col, int row) const {
        const char tile = roomTile(roomId_, col, row);
        if (const RoomDoor* door = doorAt(roomId_, col, row)) {
            return doorUnlocked(*door) && !solidTile(tile);
        }
        return !solidTile(tile);
    }

    void setNotice(const wchar_t* text) {
        notice_ = text;
        noticeStart_ = nowMs();
    }

    // Everything the run remembers between rooms.
    void resetRunState() {
        roomId_ = 0;
        key_ = false;
        keyTaken_ = false;
        levers_ = {};
        leverSolved_ = false;
        plateStep_ = 0;
        plateSolved_ = false;
        plateFlash_ = 0;
        notice_.clear();
        noticeStart_ = 0;
    }

    void resetGame() {
        resetRunState();
        phase_ = GamePhase::Play;
        const RoomDef& start = kRooms[0];
        walker_ = Walker{};
        walker_.tileX = walker_.toX = start.startX;
        walker_.tileY = walker_.toY = start.startY;
        walker_.dir = kFaceDown;
        lastDir_ = -1;
        if (hwnd_) SetWindowTextW(hwnd_, L"Anaise - Bethelament");
        invalidate();
    }

    void enterRoom(int room, int spawnX, int spawnY) {
        roomId_ = std::clamp(room, 0, kRoomCount - 1);
        walker_.tileX = walker_.toX = spawnX;
        walker_.tileY = walker_.toY = spawnY;
        // Step away from the doorway the pilgrim just used.
        walker_.dir = spawnY <= 1 ? kFaceDown : kFaceUp;
        walker_.moving = false;
        walker_.progress = 0.0;
        walker_.animMs = 0.0;
        walker_.pose = kStandPose;
        lastDir_ = -1;
        if (roomId_ == 3) setNotice(kRooms[3].hint);
        invalidate();
    }

    // What happens when a whole step lands on a tile. Doors travel, keys are
    // picked up, plates listen for their order, and the breach opens.
    void onArrive() {
        const int col = walker_.tileX;
        const int row = walker_.tileY;
        const char tile = roomTile(roomId_, col, row);

        if (tile == L'K' && !keyTaken_) {
            keyTaken_ = true;
            key_ = true;
            setNotice(L"The key is warm. The north door is unbarred.");
        } else if (tile >= L'1' && tile <= L'3') {
            const int index = tile - L'1';
            if (index == plateStep_) {
                ++plateStep_;
                if (plateStep_ >= 3) {
                    plateStep_ = 3;
                    if (!plateSolved_) {
                        plateSolved_ = true;
                        setNotice(L"Three plates. The north door opens.");
                    }
                } else if (index == 0) {
                    setNotice(L"One plate answers.");
                } else {
                    setNotice(L"Two plates answer.");
                }
            } else {
                // Out of order: the ossuary forgets everything but I.
                plateStep_ = index == 0 ? 1 : 0;
                plateFlash_ = nowMs();
                setNotice(index == 0 ? L"One plate answers."
                                     : L"The plates forget. Begin at I.");
            }
        } else if (tile == L'O') {
            startBreak();
            return;
        } else if (tile == L'~') {
            setNotice(L"The floor repeats under your feet.");
        }

        if (const RoomDoor* door = doorAt(roomId_, col, row)) {
            if (doorUnlocked(*door)) {
                enterRoom(door->target, door->spawnX, door->spawnY);
                return;
            }
        }
        invalidate();
    }

    // Levers are indexed left to right, top to bottom, the way they are read.
    int leverIndexAt(int col, int row) const {
        int found = -1;
        int index = 0;
        for (int y = 0; y < kRoomRows; ++y) {
            for (int x = 0; x < kRoomCols; ++x) {
                if (roomTile(roomId_, x, y) != L'L') continue;
                if (x == col && y == row) found = index;
                if (index < kLeverCount - 1) ++index;
            }
        }
        return found;
    }

    void toggleLever(int col, int row) {
        const int index = leverIndexAt(col, row);
        if (index < 0 || index >= kLeverCount) return;
        levers_[index] = !levers_[index];
        bool solved = true;
        for (int i = 0; i < kLeverCount; ++i) {
            if (levers_[i] != kLeverTarget[i]) solved = false;
        }
        if (solved && !leverSolved_) {
            leverSolved_ = true;
            setNotice(L"Three levers align. The north door opens.");
        } else if (!solved) {
            setNotice(L"The lever answers. The carving does not.");
        }
        invalidate();
    }

    int heldDirection() const {
        // Only an active window should walk; otherwise a key held while the
        // app loses focus would keep the pilgrim moving in the background.
        if (GetForegroundWindow() != hwnd_) return -1;
        auto held = [](int key) { return (GetKeyState(key) & 0x8000) != 0; };
        const bool pressed[4] = {held(VK_DOWN) || held('S'),
                                 held(VK_LEFT) || held('A'),
                                 held(VK_RIGHT) || held('D'),
                                 held(VK_UP) || held('W')};
        if (lastDir_ >= 0 && lastDir_ < 4 && pressed[lastDir_]) return lastDir_;
        for (int dir = 0; dir < 4; ++dir) {
            if (pressed[dir]) return dir;
        }
        return -1;
    }

    // Finishes any step in progress, so the pilgrim is always standing on a
    // whole tile: on focus loss, or when the room is left mid-stride.
    void stopWalker() {
        lastDir_ = -1;
        if (!walker_.moving) return;
        walker_.tileX = walker_.toX;
        walker_.tileY = walker_.toY;
        walker_.moving = false;
        walker_.progress = 0.0;
        walker_.animMs = 0.0;
        walker_.pose = kStandPose;
        invalidate();
    }

    void tickGame() {
        if (screen_ != Screen::Library) return;
        const ULONGLONG now = GetTickCount64();
        double elapsed = static_cast<double>(now - gameTickLast_);
        gameTickLast_ = now;
        if (elapsed > 120.0) elapsed = 120.0;
        if (phase_ == GamePhase::Play) {
            if (updateWalker(elapsed)) invalidate();
            return;
        }
        if (phase_ == GamePhase::Crash) {
            if (now - phaseStart_ >= kCrashAutoMs) {
                beginSearch();
            } else {
                invalidate();
            }
            return;
        }
        if (phase_ == GamePhase::Search) {
            if (now - phaseStart_ >= kSearchAutoMs) {
                beginCorridor();
            } else {
                invalidate();
            }
            return;
        }
        updateCorridor(elapsed);
        invalidate();
    }

    // One tile per step with a pause on the standing frame in between, the way
    // an RPG Maker character moves: 1-2-3-2, one step for every keypress held.
    bool updateWalker(double elapsed) {
        bool changed = false;
        const int dir = heldDirection();

        if (walker_.moving) {
            walker_.progress += elapsed / kStepMs;
            walker_.animMs += elapsed;
            if (walker_.progress >= 1.0) {
                walker_.tileX = walker_.toX;
                walker_.tileY = walker_.toY;
                walker_.progress = 0.0;
                walker_.moving = false;
                onArrive();
            }
            changed = true;
        }

        if (!walker_.moving && dir >= 0) {
            if (walker_.dir != dir) {
                walker_.dir = dir;
                changed = true;
            }
            const int nextX = walker_.tileX + kDirX[dir];
            const int nextY = walker_.tileY + kDirY[dir];
            if (roomWalkable(nextX, nextY)) {
                walker_.toX = nextX;
                walker_.toY = nextY;
                walker_.progress = 0.0;
                walker_.moving = true;
                changed = true;
            } else if (const RoomDoor* door =
                           doorAt(roomId_, nextX, nextY)) {
                // The door is there but locked: say what it wants.
                if (!doorUnlocked(*door)) {
                    if (door->lock == kDoorKey && !key_) {
                        setNotice(L"The north door is barred. A key would open it.");
                    } else if (door->lock == kDoorLevers) {
                        setNotice(L"The door waits for the three levers.");
                    } else if (door->lock == kDoorPlates) {
                        setNotice(L"The door waits for the plates, in order.");
                    }
                }
            }
        }

        if (walker_.moving) {
            static constexpr int kPoses[4] = {0, 1, 2, 1};
            const int pose =
                kPoses[static_cast<int>(walker_.animMs / kPoseMs) % 4];
            if (pose != walker_.pose) {
                walker_.pose = pose;
                changed = true;
            }
        } else {
            lastDir_ = dir;
            walker_.animMs = 0.0;
            if (walker_.pose != kStandPose) {
                walker_.pose = kStandPose;
                changed = true;
            }
        }
        return changed;
    }

    // ---------------------------------------------------------------- mouse
    void onMouseMove(int x, int y) {
        if (gods_.active()) return;
        if (hwnd_ && !mouseTracking_) {
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd_;
            TrackMouseEvent(&track);
            mouseTracking_ = true;
        }

        if (pressedIcon_ >= 0) {
            const int dx = x - int(dragStart_.x), dy = y - int(dragStart_.y);
            if (!iconDragged_ && (std::abs(dx) > px(4) || std::abs(dy) > px(4))) iconDragged_ = true;
            if (iconDragged_) {
                const RECT desk = menuLayout(shellClient().right, shellClient().bottom).desktop;
                const int w = iconDragBounds_.right - iconDragBounds_.left;
                const int h = iconDragBounds_.bottom - iconDragBounds_.top;
                const int left = std::clamp(int(iconDragBounds_.left) + dx,
                    int(desk.left), std::max(int(desk.left), int(desk.right) - w));
                const int top = std::clamp(int(iconDragBounds_.top) + dy,
                    int(desk.top), std::max(int(desk.top), int(desk.bottom) - h));
                iconPositions_[pressedIcon_] = POINT{MulDiv(left - desk.left, 96, dpi_),
                                                      MulDiv(top - desk.top, 96, dpi_)};
                iconPlaced_[pressedIcon_] = true;
                invalidate();
            }
            return;
        }
        if (draggingApp_ >= 0) {
            const RECT desk = menuLayout(shellClient().right, shellClient().bottom).desktop;
            RECT bounds = dragBounds_;
            const int w = bounds.right - bounds.left, h = bounds.bottom - bounds.top;
            int left = std::clamp(int(dragBounds_.left) + x - int(dragStart_.x),
                                  int(desk.left), int(desk.right) - w);
            int top = std::clamp(int(dragBounds_.top) + y - int(dragStart_.y),
                                 int(desk.top), int(desk.bottom) - h);
            appWindows_[draggingApp_].bounds = RECT{left, top, left + w, top + h};
            invalidate();
            return;
        }
        if (screen_ == Screen::Tentacle && tentacle_.dragging()) {
            int cx = x, cy = y;
            tentaclePoint(cx, cy);
            tentacle_.mouseMove(cx, cy);
            invalidate();
            return;
        }
        const MenuHit shellHit = menuHitAt(x, y);
        if (!painting_ && (screen_ == Screen::Menu || shellHit.part != MenuPart::None)) {
            const MenuHit hit = shellHit;
            bool changed = hit.part != menuHot_ || hit.index != menuHotIndex_;
            menuHot_ = hit.part;
            menuHotIndex_ = hit.index;
            if (hit.part == MenuPart::Icon && hit.index != menuIndex_) {
                menuIndex_ = hit.index;
                changed = true;
            }
            if (changed) invalidate();
            return;
        }

        if (menuHot_ != MenuPart::None) {
            menuHot_ = MenuPart::None;
            menuHotIndex_ = -1;
            invalidate();
        }
        if (!contentPoint(x, y)) {
            if (hoverValid_ || browserHover_ >= 0) {
                hoverValid_ = false;
                browserHover_ = -1;
                invalidate();
            }
            return;
        }
        if (screen_ == Screen::Tentacle) {
            tentacle_.mouseMove(x, y);
            return;
        }
        if (screen_ == Screen::Browser) {
            const int hit = browserHitAt(x, y);
            if (hit != browserHover_) {
                browserHover_ = hit;
                invalidate();
            }
            return;
        }

        if (screen_ != Screen::Paint) return;

        if (painting_) paintTo(x, y);

        int col = 0;
        int row = 0;
        const bool inside = paintCellAt(x, y, col, row);
        if (!inside) {
            if (hoverValid_) {
                hoverValid_ = false;
                invalidate();
            }
        } else if (!hoverValid_ || col != hoverCol_ || row != hoverRow_) {
            hoverValid_ = true;
            hoverCol_ = col;
            hoverRow_ = row;
            invalidate();
        }
    }

    void onMouseDown(bool leftButton, int x, int y) {
        if (hwnd_) SetFocus(hwnd_);

        if (gods_.active()) {
            if (leftButton) gods_.key('J');
            else godsMouseGuard_ = true;
            audio_.effects(gods_.takeSounds());
            invalidate();
            return;
        }
        if (leftButton && godsTitleHit(x, y)) {
            const RECT client=shellClient();
            gods_.clickTitle(nowMs(), x, y, client.right, client.bottom);
            audio_.effects(gods_.takeSounds());
            if (gods_.active()) { enterMenu(); audio_.play(anaise::Music::Hymn); }
            invalidate();
            return;
        }

        if (screen_ == Screen::Splash) {
            enterMenu();
            return;
        }
        if ((screen_ == Screen::Library && phase_ != GamePhase::Play) ||
            screen_ == Screen::Crash || screen_ == Screen::Search ||
            screen_ == Screen::Corridor) {
            if (!leftButton) return;
            if (phase_ == GamePhase::Crash) beginSearch();
            else if (phase_ == GamePhase::Search) beginCorridor();
            else if (phase_ == GamePhase::Corridor) attackSchool();
            return;
        }
        const MenuHit hit = menuHitAt(x, y);
        if (hit.index >= 0 && (hit.part == MenuPart::WindowClose ||
            hit.part == MenuPart::WindowMax || hit.part == MenuPart::WindowMin)) focusApp(hit.index);
        if ((screen_ == Screen::Menu && windowAt(x, y) < 0) || hit.part != MenuPart::None) {
            if (!leftButton) return;
            if (startMenuVisible_ && hit.part != MenuPart::Start &&
                hit.part != MenuPart::StartItem) {
                startMenuVisible_ = false;
                invalidate();
                return;
            }
            switch (hit.part) {
                case MenuPart::Icon:
                    enterMenu();
                    menuIndex_ = hit.index;
                    pressedIcon_ = hit.index;
                    iconDragged_ = false;
                    dragStart_ = POINT{x, y};
                    iconDragBounds_ = menuLayout(shellClient().right, shellClient().bottom).iconHit[hit.index];
                    if (hwnd_) SetCapture(hwnd_);
                    invalidate();
                    break;
                case MenuPart::Start:
                    startMenuVisible_ = !startMenuVisible_;
                    invalidate();
                    break;
                case MenuPart::StartItem:
                    menuIndex_ = hit.index;
                    startMenuVisible_ = false;
                    activateMenuItem();
                    break;
                case MenuPart::TaskChip:
                    menuIndex_ = hit.index;
                    if (activeApp() == hit.index) dismissApp(false);
                    else focusApp(hit.index);
                    break;
                case MenuPart::WindowClose:
                    dismissApp(true);
                    break;
                case MenuPart::WindowMax:
                    if (activeApp() >= 0) {
                        appWindows_[activeApp()].maximized = !appWindows_[activeApp()].maximized;
                        invalidate();
                    }
                    break;
                case MenuPart::WindowMin:
                    dismissApp(false);
                    break;
                case MenuPart::Signal:
                    audio_.toggleMuted();
                    invalidate();
                    break;
                default:
                    break;
            }
            return;
        }
        for (auto it = windowOrder_.rbegin(); it != windowOrder_.rend(); ++it) {
            const int index = *it;
            if (!appWindows_[index].open || appWindows_[index].minimized) continue;
            const MenuLayout layout = appWindowLayout(index);
            if (!PtInRect(&layout.window, POINT{x, y})) continue;
            focusApp(index);
            if (leftButton && PtInRect(&layout.windowTitle, POINT{x, y}) &&
                !appWindows_[index].maximized) {
                draggingApp_ = index;
                dragStart_ = POINT{x, y};
                dragBounds_ = layout.window;
                if (hwnd_) SetCapture(hwnd_);
                return;
            }
            break;
        }
        if (!contentPoint(x, y)) {
            enterMenu();
            return;
        }
        if (screen_ == Screen::Tentacle) {
            tentacle_.mouseDown(x, y, leftButton);
            audio_.tentacle(tentacle_.takeSounds());
            if (leftButton && tentacle_.dragging() && hwnd_) SetCapture(hwnd_);
            invalidate();
            return;
        }
        if (screen_ == Screen::Browser) {
            if (!leftButton) return;
            const int hit = browserHitAt(x, y);
            if (hit >= 0 && hit < 6) browserSelectTab(hit);
            else if (hit >= 50 && hit < 56) browserCloseTab(hit - 50);
            else if (hit >= 100) {
                auto rows = browserRows();
                if (hit - 100 < static_cast<int>(rows.size())) browserGo(rows[hit - 100]);
            }
            else if (hit == kBrowserHitBack) browserHistory(-1);
            else if (hit == kBrowserHitForward) browserHistory(1);
            else if (hit == kBrowserHitLink) browserFollowLink();
            else if (hit == kBrowserHitReload) browserReload();
            else if (hit == 30) browserEditAddress();
            else if (hit == 31) browserNewTab();
            else if (hit == 32 || hit == 33 || hit == 34) {
                browserPanel_ = browserPanel_ == hit - 31 ? 0 : hit - 31;
                browserPanelScroll_ = 0;
                browserEditing_ = false; browserQuery_.clear();
            }
            else if (hit == 35) browserBookmarks_[browserPage_] = !browserBookmarks_[browserPage_];
            else browserEditing_ = false;
            invalidate();
            return;
        }
        if (screen_ != Screen::Paint) return;

        int col = 0;
        int row = 0;
        if (!paintCellAt(x, y, col, row)) return;

        pushUndo();
        painting_ = true;
        lastPaintValid_ = false;
        erasing_ = !leftButton;
        if (hwnd_) SetCapture(hwnd_);
        paintTo(x, y);
    }

    void onMouseUp() {
        godsMouseGuard_ = false;
        if (screen_ == Screen::Tentacle) tentacle_.mouseUp();
        const int clickedIcon = !iconDragged_ ? pressedIcon_ : -1;
        pressedIcon_ = -1;
        iconDragged_ = false;
        draggingApp_ = -1;
        painting_ = false;
        lastPaintValid_ = false;
        if (hwnd_ && GetCapture() == hwnd_) ReleaseCapture();
        invalidate();
        if (clickedIcon >= 0) {
            menuIndex_ = clickedIcon;
            activateMenuItem();
        }
    }

    MenuHit menuHitAt(int x, int y) const {
        const RECT client = shellClient();
        const MenuLayout layout = menuLayout(client.right, client.bottom);
        const POINT point{x, y};
        auto inside = [&](const RECT& rect) {
            return PtInRect(&rect, point) != 0;
        };

        if (startMenuVisible_) {
            for (int i = 0; i < kMenuItems; ++i) {
                if (inside(layout.startItem[i])) {
                    return MenuHit{MenuPart::StartItem, i};
                }
            }
        }
        for (auto it = windowOrder_.rbegin(); it != windowOrder_.rend(); ++it) {
            int i = *it;
            if (!appWindows_[i].open || appWindows_[i].minimized) continue;
            const MenuLayout win = appWindowLayout(i);
            if (!inside(win.window)) continue;
            if (inside(win.windowClose)) return MenuHit{MenuPart::WindowClose, i};
            if (inside(win.windowMax)) return MenuHit{MenuPart::WindowMax, i};
            if (inside(win.windowMin)) return MenuHit{MenuPart::WindowMin, i};
            return MenuHit{};
        }
        if (inside(layout.iconHit[menuIndex_])) return MenuHit{MenuPart::Icon, menuIndex_};
        for (int i = kMenuItems - 1; i >= 0; --i) {
            if (inside(layout.iconHit[i])) return MenuHit{MenuPart::Icon, i};
        }
        if (inside(layout.startButton)) return MenuHit{MenuPart::Start, -1};
        for (int i = 0; i < kMenuItems; ++i) {
            if (i < kAppCount && appWindows_[i].open && inside(layout.taskChip[i])) {
                return MenuHit{MenuPart::TaskChip, i};
            }
        }
        if (inside(layout.signal)) return MenuHit{MenuPart::Signal, -1};
        return MenuHit{};
    }

    MenuLayout menuLayout(int width, int height) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };

        MenuLayout layout{};
        const int margin = 0;
        const int bezel = px(12);
        layout.monitor =
            RECT{margin, margin, width - margin, height - margin};
        layout.screen = RECT{margin + bezel, margin + bezel,
                             width - margin - bezel, height - margin - bezel};
        if (layout.screen.right <= layout.screen.left + px(40) ||
            layout.screen.bottom <= layout.screen.top + px(40)) {
            layout.screen = RECT{0, 0, width, height};
            layout.monitor = layout.screen;
        }

        const int taskHeight = px(30);
        layout.taskBar = RECT{layout.screen.left,
                              layout.screen.bottom - taskHeight,
                              layout.screen.right, layout.screen.bottom};
        layout.desktop = RECT{layout.screen.left, layout.screen.top,
                              layout.screen.right, layout.taskBar.top};

        const int tile = px(48);
        const int labelHeight = px(18);
        const int hitWidth = px(104);
        const int hitHeight = tile + px(4) + labelHeight;
        const RECT& desk = layout.desktop;
        for (int i = 0; i < kMenuItems; ++i) {
            // Default icons stack down the left and wrap into a new column.
            const int perColumn = std::max(1, (MulDiv(int(desk.bottom - desk.top), 96, dpi_) - 12) / 90);
            const int desiredX = iconPlaced_[i] ? px(iconPositions_[i].x) : px(12 + (i / perColumn) * 110);
            const int desiredY = iconPlaced_[i] ? px(iconPositions_[i].y) : px(12 + (i % perColumn) * 90);
            const int left = desk.left + std::clamp(desiredX, 0,
                std::max(0, int(desk.right - desk.left) - hitWidth));
            const int top = desk.top + std::clamp(desiredY, 0,
                std::max(0, int(desk.bottom - desk.top) - hitHeight));
            layout.iconHit[i] = RECT{left, top, left + hitWidth, top + hitHeight};
            const int tileLeft = left + (hitWidth - tile) / 2;
            layout.iconTile[i] = RECT{tileLeft, top, tileLeft + tile, top + tile};
            layout.iconLabel[i] = RECT{left, top + tile + px(4), left + hitWidth,
                                      top + hitHeight};
        }

        // The taskbar: start plate, then one chip per app, then the clock and
        // the signal bars at the far right.
        const RECT& bar = layout.taskBar;
        layout.startButton =
            RECT{bar.left + px(6), bar.top + px(4),
                 bar.left + px(6) + px(30), bar.bottom - px(4)};
        int chipLeft = layout.startButton.right + px(10);
        for (int i = 0; i < kAppCount; ++i) {
            if (!appWindows_[i].open) continue;
            const int chipWidth = static_cast<int>(std::wcslen(kMenuLabels[i])) *
                                      px(7) +
                                  px(18);
            layout.taskChip[i] =
                RECT{chipLeft, bar.top + px(4), chipLeft + chipWidth,
                     bar.bottom - px(4)};
            chipLeft = layout.taskChip[i].right + px(5);
        }
        const int clockReserve = px(35);
        layout.signal =
            RECT{bar.right - px(10) - clockReserve - px(30), bar.top + px(4),
                 bar.right - px(10) - clockReserve, bar.bottom - px(4)};

        // The start menu sits directly above the start plate.
        const int startItemHeight = px(22);
        const int startPad = px(4);
        const int startWidth = px(150);
        const int startHeight =
            kMenuItems * startItemHeight + startPad * 2;
        layout.startMenu =
            RECT{layout.startButton.left, bar.top - px(4) - startHeight,
                 layout.startButton.left + startWidth, bar.top - px(4)};
        for (int i = 0; i < kMenuItems; ++i) {
            const int top = layout.startMenu.top + startPad +
                            i * startItemHeight;
            layout.startItem[i] =
                RECT{layout.startMenu.left + startPad, top,
                     layout.startMenu.right - startPad, top + startItemHeight};
        }
        return layout;
    }

    RECT menuItemRect(int index, int width, int height) const {
        const MenuLayout layout = menuLayout(width, height);
        if (index < 0 || index >= kMenuItems) return RECT{};
        return layout.iconHit[index];
    }

    // ----------------------------------------------------------------- veil
    BrowserLayout browserLayout(int width, int height) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };

        BrowserLayout layout;
        const int margin = px(10);
        const int top = px(8);
        const int bottom = std::max(top + px(230), height - px(8));
        layout.window = RECT{margin, top, width - margin, bottom};
        layout.title =
            RECT{layout.window.left, layout.window.top, layout.window.right,
                 layout.window.top};

        const int tabHeight = px(28);
        const int tabWidth = std::min(px(142), static_cast<int>((layout.window.right - layout.window.left - px(42)) / browserTabs_.size()));
        for (int i = 0; i < static_cast<int>(browserTabs_.size()); ++i) {
            const int left = layout.window.left + px(6) + i * tabWidth;
            layout.tabs[i] = RECT{left, top + px(3), left + tabWidth - px(3), top + tabHeight};
        }
        const int plus = layout.tabs[browserTabs_.size() - 1].right + px(3);
        layout.newTab = RECT{plus, top + px(3), plus + px(24), top + tabHeight};
        layout.tabsBottom = top + tabHeight;

        const int toolbarTop = layout.tabsBottom;
        const int toolbarBottom = toolbarTop + px(32);
        const int control = px(20);
        const int controlTop = toolbarTop + (px(32) - control) / 2;
        const int controlLeft = layout.window.left + px(8);
        layout.back =
            RECT{controlLeft, controlTop, controlLeft + control,
                 controlTop + control};
        layout.forward =
            RECT{layout.back.right + px(4), controlTop,
                 layout.back.right + px(4) + control, controlTop + control};
        layout.reload =
            RECT{layout.forward.right + px(4), controlTop,
                 layout.forward.right + px(4) + control, controlTop + control};

        const int addressTop = toolbarTop + px(6);
        layout.address =
            RECT{layout.reload.right + px(10), addressTop,
                 layout.window.right - px(34), addressTop + px(20)};

        layout.star = RECT{layout.window.right - px(28), addressTop, layout.window.right - px(6), addressTop + px(20)};
        layout.directory = RECT{layout.window.left + px(8), toolbarBottom + px(3), layout.window.left + px(100), toolbarBottom + px(25)};
        layout.history = RECT{layout.directory.right + px(4), layout.directory.top, layout.directory.right + px(84), layout.directory.bottom};
        layout.bookmarks = RECT{layout.history.right + px(4), layout.directory.top, layout.history.right + px(104), layout.directory.bottom};
        layout.content = RECT{layout.window.left + 1, toolbarBottom + px(29),
                              layout.window.right - 1, layout.window.bottom - 1};
        return layout;
    }

    RECT browserLinkRect(const BrowserLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const int pad = px(22);
        const int width = px(236);
        const int height = px(26);
        return RECT{layout.content.right - pad - width,
                    layout.content.bottom - pad - height,
                    layout.content.right - pad,
                    layout.content.bottom - pad};
    }

    int browserHitAt(int x, int y) const {
        const RECT client = contentClient(2);
        const BrowserLayout layout =
            browserLayout(client.right, client.bottom);
        const POINT point{x, y};
        auto inside = [&](const RECT& rect) {
            return PtInRect(&rect, point) != 0;
        };
        for (int i = 0; i < static_cast<int>(browserTabs_.size()); ++i) {
            if (inside(layout.tabs[i])) return x >= layout.tabs[i].right - px(18) ? 50 + i : i;
        }
        if (inside(layout.address)) return 30;
        if (inside(layout.newTab)) return 31;
        if (inside(layout.directory)) return 32;
        if (inside(layout.history)) return 33;
        if (inside(layout.bookmarks)) return 34;
        if (inside(layout.star)) return 35;
        if (browserPanel_ && inside(layout.content)) {
            int row = (y - layout.content.top - px(66) + browserPanelScroll_) / px(30);
            if (y >= layout.content.top + px(66) && row >= 0 &&
                y < layout.content.bottom && row < static_cast<int>(browserRows().size())) return 100 + row;
        }
        if (inside(layout.back)) return kBrowserHitBack;
        if (inside(layout.forward)) return kBrowserHitForward;
        if (inside(layout.reload)) return kBrowserHitReload;
        if (!browserPanel_ && inside(browserLinkRect(layout))) return kBrowserHitLink;
        return -1;
    }

    // -------------------------------------------------------------- painting
    GridLayout paintLayout(int width, int height) const {
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int logoCell = std::max(2, MulDiv(4, scale, 96));
        const int headerBottom =
            margin + logoCell * kLogoGrid + MulDiv(18, scale, 96) + 1;
        const int topLimit = headerBottom + MulDiv(20, scale, 96);
        const int bottomLimit = height - MulDiv(58, scale, 96);
        const int availableWidth = width - margin * 2;
        const int availableHeight = bottomLimit - topLimit;
        int cell = std::min(availableWidth / kPaintCols,
                            availableHeight / kPaintRows);
        cell = std::max(4, cell);
        const int gridWidth = cell * kPaintCols;
        const int gridHeight = cell * kPaintRows;
        GridLayout layout;
        layout.cell = cell;
        layout.left = (width - gridWidth) / 2;
        layout.top = topLimit + std::max(0, (availableHeight - gridHeight) / 2);
        return layout;
    }

    bool paintCellAt(int x, int y, int& col, int& row) const {
        const RECT client = contentClient(1);
        const GridLayout layout = paintLayout(client.right, client.bottom);
        if (layout.cell <= 0) return false;
        if (x < layout.left || y < layout.top) return false;
        const int localX = x - layout.left;
        const int localY = y - layout.top;
        col = localX / layout.cell;
        row = localY / layout.cell;
        return col >= 0 && col < kPaintCols && row >= 0 && row < kPaintRows;
    }

    void paintTo(int x, int y) {
        int col = 0;
        int row = 0;
        if (!paintCellAt(x, y, col, row)) {
            lastPaintValid_ = false;
            return;
        }
        if (lastPaintValid_) {
            paintLine(lastPaintCol_, lastPaintRow_, col, row, erasing_);
        } else {
            stampBrush(col, row, erasing_);
        }
        lastPaintCol_ = col;
        lastPaintRow_ = row;
        lastPaintValid_ = true;
        invalidate();
    }

    void stampBrush(int col, int row, bool erase) {
        const int start = -(paintBrush_ - 1) / 2;
        for (int dr = 0; dr < paintBrush_; ++dr) {
            for (int dc = 0; dc < paintBrush_; ++dc) {
                const int targetCol = col + start + dc;
                const int targetRow = row + start + dr;
                if (targetCol < 0 || targetCol >= kPaintCols ||
                    targetRow < 0 || targetRow >= kPaintRows) {
                    continue;
                }
                canvas_[targetRow][targetCol] = !erase;
            }
        }
    }

    void paintLine(int x0, int y0, int x1, int y1, bool erase) {
        int dx = std::abs(x1 - x0);
        int sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            stampBrush(x0, y0, erase);
            if (x0 == x1 && y0 == y1) break;
            const int twice = error * 2;
            if (twice >= dy) {
                error += dy;
                x0 += sx;
            }
            if (twice <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }

    void pushUndo() {
        if (undo_.size() >= 64) undo_.erase(undo_.begin());
        undo_.push_back(canvas_);
        // A fresh edit makes the undone future unreachable, as in any editor.
        redo_.clear();
    }

    void undoCanvas() {
        if (undo_.empty()) return;
        if (redo_.size() >= 64) redo_.erase(redo_.begin());
        redo_.push_back(canvas_);
        canvas_ = undo_.back();
        undo_.pop_back();
        invalidate();
    }

    void redoCanvas() {
        if (redo_.empty()) return;
        if (undo_.size() >= 64) undo_.erase(undo_.begin());
        undo_.push_back(canvas_);
        canvas_ = redo_.back();
        redo_.pop_back();
        invalidate();
    }

    void clearCanvas() {
        pushUndo();
        for (auto& row : canvas_) row.fill(false);
        invalidate();
    }

    // ------------------------------------------------------------ fullscreen
    void setFullscreen(bool on) {
        if (on == fullscreen_) return;
        if (on) {
            placement_.length = sizeof(placement_);
            GetWindowPlacement(hwnd_, &placement_);
            windowedStyle_ = static_cast<LONG>(
                GetWindowLongPtrW(hwnd_, GWL_STYLE));
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST),
                            &info);
            SetWindowLongPtrW(hwnd_, GWL_STYLE,
                              WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
            SetWindowPos(hwnd_, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
                         info.rcMonitor.right - info.rcMonitor.left,
                         info.rcMonitor.bottom - info.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            fullscreen_ = true;
        } else {
            SetWindowLongPtrW(hwnd_, GWL_STYLE, windowedStyle_);
            SetWindowPlacement(hwnd_, &placement_);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                             SWP_FRAMECHANGED);
            fullscreen_ = false;
            applyMonochromeTitleBar(hwnd_);
        }
        invalidate();
    }

    // --------------------------------------------------------------- drawing
    RECT godsTitleRect() {
        ensureFonts();
        HDC dc=GetDC(nullptr);
        const int width=textWidth(dc,dedicationFont_,kDesktopDedication);
        const int height=textHeight(dc,dedicationFont_);
        ReleaseDC(nullptr,dc);
        const RECT client=shellClient();
        const auto layout=menuLayout(client.right,client.bottom);
        const int right=layout.desktop.right-px(18),top=layout.desktop.top+px(16);
        return RECT{right-width,top,right,top+height};
    }
    bool godsTitleHit(int x,int y) {
        if (screen_==Screen::Splash || gods_.active() || startMenuVisible_ ||
            (screen_==Screen::Library&&phase_!=GamePhase::Play) ||
            screen_==Screen::Crash || screen_==Screen::Search || screen_==Screen::Corridor ||
            windowAt(x,y)>=0) return false;
        const RECT hit=godsTitleRect();return PtInRect(&hit,POINT{x,y});
    }

    // Headless captures run on a virtual clock, so the same --time always
    // produces the same frame.
    ULONGLONG nowMs() const {
        return headless_ ? headlessNowMs_ : GetTickCount64();
    }

    void releaseFonts() {
        if (titleFont_) DeleteObject(titleFont_);
        if (headingFont_) DeleteObject(headingFont_);
        if (bodyFont_) DeleteObject(bodyFont_);
        if (smallFont_) DeleteObject(smallFont_);
        if (dedicationFont_) DeleteObject(dedicationFont_);
        titleFont_ = nullptr;
        headingFont_ = nullptr;
        bodyFont_ = nullptr;
        smallFont_ = nullptr;
        dedicationFont_ = nullptr;
        fontsDpi_ = 0;
    }

    void ensureFonts() {
        if (fontsDpi_ == dpi_ && titleFont_ && headingFont_ && bodyFont_ &&
            smallFont_ && dedicationFont_) {
            return;
        }
        releaseFonts();

        auto makeFont = [this](int points, const wchar_t* face,
                               BYTE quality = ANTIALIASED_QUALITY) {
            LOGFONTW lf{};
            lf.lfWeight = FW_NORMAL;
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfQuality = quality;
            lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
            lf.lfHeight = -MulDiv(points, static_cast<int>(dpi_), 72);
            lstrcpyW(lf.lfFaceName, face);
            return CreateFontIndirectW(&lf);
        };

        const wchar_t* displayFace = embeddedFont_ ? kTitleFace : L"Georgia";
        titleFont_ = makeFont(40, displayFace);
        headingFont_ = makeFont(24, displayFace);
        bodyFont_ = makeFont(15, L"Consolas");
        smallFont_ = makeFont(10, L"Consolas");
        dedicationFont_ =
            makeFont(30, blackletterFontResource_ ? kBlackletterFace : L"Georgia");
        if (!bodyFont_) bodyFont_ = makeFont(15, L"Courier New");
        if (!smallFont_) smallFont_ = makeFont(10, L"Courier New");
        fontsDpi_ = dpi_;
    }

    int textWidth(HDC dc, HFONT font, const std::wstring& text) const {
        HGDIOBJ old = SelectObject(dc, font);
        SIZE size{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()),
                              &size);
        SelectObject(dc, old);
        return size.cx;
    }

    int textHeight(HDC dc, HFONT font) const {
        HGDIOBJ old = SelectObject(dc, font);
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);
        SelectObject(dc, old);
        return metrics.tmHeight;
    }

    void drawTextAt(HDC dc, HFONT font, const std::wstring& text, int x, int y,
                    COLORREF color, UINT align = TA_LEFT | TA_TOP) const {
        if (text.empty()) return;
        HGDIOBJ old = SelectObject(dc, font);
        SetTextColor(dc, color);
        SetTextAlign(dc, align);
        TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
        SetTextAlign(dc, TA_LEFT | TA_TOP);
        SelectObject(dc, old);
    }

    void drawTextCentered(HDC dc, HFONT font, const std::wstring& text, int x,
                          int y, COLORREF color) const {
        drawTextAt(dc, font, text, x, y, color, TA_CENTER | TA_TOP);
    }

    // Lays type onto the desktop by flipping what is already there: ring
    // pattern and scanline inside each letterform come out inverted, while the
    // untouched background stays exactly as it was. A 32-bit mask keeps the
    // curves of the blackletter face, and SRCINVERT keeps the palette binary.
    void drawInvertedTextRight(HDC dc, HFONT font, const std::wstring& text,
                               int right, int top) const {
        if (text.empty()) return;
        const int pad = 3;
        const int width = textWidth(dc, font, text) + pad * 2;
        const int height = textHeight(dc, font);
        if (width <= pad * 2 || height <= 0) return;

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;  // top-down rows
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP mask =
            CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!mask || !bits) {
            if (mask) DeleteObject(mask);
            return;
        }

        HDC maskDc = CreateCompatibleDC(dc);
        HGDIOBJ oldMask = SelectObject(maskDc, mask);
        RECT area{0, 0, width, height};
        fillColor(maskDc, area, kBlack);
        SetBkMode(maskDc, TRANSPARENT);
        drawTextAt(maskDc, font, text, pad, 0, kWhite);

        // Anti-aliasing gives the face its shape; thresholding gives GDI's
        // greys back to the two-colour palette before anything is flipped.
        auto* pixels = static_cast<std::uint32_t*>(bits);
        for (int i = 0; i < width * height; ++i) {
            const std::uint32_t pixel = pixels[i];
            const int luma = static_cast<int>(pixel & 0xFF) +
                             static_cast<int>((pixel >> 8) & 0xFF) +
                             static_cast<int>((pixel >> 16) & 0xFF);
            pixels[i] = luma >= 384 ? 0x00FFFFFFu : 0x00000000u;
        }

        // White source pixels invert the destination; black ones leave it.
        SetTextColor(maskDc, kWhite);
        SetBkColor(maskDc, kBlack);
        BitBlt(dc, right - width, top, width, height, maskDc, 0, 0, SRCINVERT);

        SelectObject(maskDc, oldMask);
        DeleteDC(maskDc);
        DeleteObject(mask);
    }

    // The splash title, lit only down to `litPercent` of its height so it can
    // sputter on like a failing tube instead of simply appearing.
    void drawSplashTitle(HDC dc, int width, int titleY, int titleHeight,
                         int litPercent) const {
        if (litPercent <= 0) return;
        if (litPercent >= 100) {
            drawTextCentered(dc, titleFont_, L"ANAISE", width / 2, titleY,
                             kWhite);
            return;
        }
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 0, titleY, width,
                          titleY + titleHeight * litPercent / 100);
        drawTextCentered(dc, titleFont_, L"ANAISE", width / 2, titleY, kWhite);
        RestoreDC(dc, saved);
    }

    void fillColor(HDC dc, const RECT& rect, COLORREF color) const {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    void strokeRect(HDC dc, const RECT& rect, COLORREF color,
                    int thickness = 1) const {
        HPEN pen = CreatePen(PS_SOLID, thickness, color);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    void drawLine(HDC dc, int x1, int y1, int x2, int y2,
                  COLORREF color = kWhite) const {
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, x1, y1, nullptr);
        LineTo(dc, x2, y2);
        SelectObject(dc, old);
        DeleteObject(pen);
    }

    void drawLogo(HDC dc, int left, int top, int cell,
                  COLORREF color = kWhite) const {
        HBRUSH brush = CreateSolidBrush(color);
        for (int row = 0; row < kLogoGrid; ++row) {
            for (int col = 0; col < kLogoGrid; ++col) {
                if (kLogo[row][col] != L'#') continue;
                RECT pixel{left + col * cell, top + row * cell,
                           left + (col + 1) * cell, top + (row + 1) * cell};
                FillRect(dc, &pixel, brush);
            }
        }
        DeleteObject(brush);
    }

    // A field of white dust at a fixed density. Everything in the shell is
    // built from this instead of greys, so the palette stays black and white.
    void fillDither(HDC dc, const RECT& rect, int level) const {
        if (rect.right <= rect.left || rect.bottom <= rect.top) return;
        if (level <= 0) {
            fillColor(dc, rect, kBlack);
            return;
        }
        if (level >= 64) {
            fillColor(dc, rect, kWhite);
            return;
        }
        static constexpr int kBayer[8][8] = {
            { 0, 32,  8, 40,  2, 34, 10, 42},
            {48, 16, 56, 24, 50, 18, 58, 26},
            {12, 44,  4, 36, 14, 46,  6, 38},
            {60, 28, 52, 20, 62, 30, 54, 22},
            { 3, 35, 11, 43,  1, 33,  9, 41},
            {51, 19, 59, 27, 49, 17, 57, 25},
            {15, 47,  7, 39, 13, 45,  5, 37},
            {63, 31, 55, 23, 61, 29, 53, 21},
        };
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = 24;
        info.bmiHeader.biHeight = -24;  // top-down rows
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP pattern =
            CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!pattern || !bits) {
            if (pattern) DeleteObject(pattern);
            return;
        }
        auto* pixels = static_cast<std::uint32_t*>(bits);
        for (int row = 0; row < 24; ++row) {
            for (int col = 0; col < 24; ++col) {
                pixels[row * 24 + col] =
                    kBayer[row / 3][col / 3] < level ? 0x00FFFFFFu : 0x00000000u;
            }
        }
        HBRUSH brush = CreatePatternBrush(pattern);
        if (brush) {
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        }
        DeleteObject(pattern);
    }

    void drawGlyph(HDC dc, const wchar_t* const* rows, int left, int top,
                   int cell, COLORREF color) const {
        if (cell <= 0) return;
        HBRUSH brush = CreateSolidBrush(color);
        for (int row = 0; row < 16; ++row) {
            for (int col = 0; col < 16; ++col) {
                if (rows[row][col] != L'#') continue;
                RECT pixel{left + col * cell, top + row * cell,
                           left + (col + 1) * cell, top + (row + 1) * cell};
                FillRect(dc, &pixel, brush);
            }
        }
        DeleteObject(brush);
    }

    void drawDashedRect(HDC dc, const RECT& rect, int dash, int gap,
                        COLORREF color = kWhite) const {
        dash = std::max(1, dash);
        gap = std::max(1, gap);
        for (int x = rect.left; x < rect.right; x += dash + gap) {
            const int end = std::min(static_cast<int>(rect.right), x + dash);
            drawLine(dc, x, rect.top, end, rect.top, color);
            drawLine(dc, x, rect.bottom - 1, end, rect.bottom - 1, color);
        }
        for (int y = rect.top; y < rect.bottom; y += dash + gap) {
            const int end = std::min(static_cast<int>(rect.bottom), y + dash);
            drawLine(dc, rect.left, y, rect.left, end, color);
            drawLine(dc, rect.right - 1, y, rect.right - 1, end, color);
        }
    }

    void drawMenuIcon(HDC dc, int index, const RECT& tile,
                      bool selected) const {
        fillColor(dc, tile, kBlack);
        if (selected) strokeRect(dc, tile, kWhite, 1);
        const int cell = std::max(1, (static_cast<int>(tile.right - tile.left) - MulDiv(8, dpi_, 96)) / 16);
        const int glyph = cell * 16;
        drawGlyph(dc, appIconRows(index), tile.left + (tile.right - tile.left - glyph) / 2,
                  tile.top + (tile.bottom - tile.top - glyph) / 2, cell, kWhite);
    }

    // The clock follows the machine it runs on, except in headless captures,
    // where it is fixed so a frame stays reproducible.
    std::wstring clockText() const {
        if (headless_) return L"03:33";
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t text[8]{};
        wsprintfW(text, L"%02u:%02u", time.wHour, time.wMinute);
        return text;
    }

    bool hot(MenuPart part, int index = -1) const {
        return menuHot_ == part && (index < 0 || menuHotIndex_ == index);
    }

    // A dotted occult seal, used as a watermark inside the app window. It is
    // built from single white pixels and short dashes, never a filled shape, so
    // it reads as faint etched light instead of a grey wash.
    void drawSigil(HDC dc, int cx, int cy, int radius) const {
        if (radius < 8) return;
        HBRUSH brush = CreateSolidBrush(kWhite);
        const int dotSize = std::max(1, MulDiv(2, static_cast<int>(dpi_), 96));
        const int half = dotSize / 2;
        auto dot = [&](double x, double y) {
            RECT pixel{static_cast<int>(std::lround(x)) - half,
                       static_cast<int>(std::lround(y)) - half,
                       static_cast<int>(std::lround(x)) - half + dotSize,
                       static_cast<int>(std::lround(y)) - half + dotSize};
            FillRect(dc, &pixel, brush);
        };
        const double pi = kPi;
        const int dots = std::max(32, radius * 2);
        for (int i = 0; i < dots; ++i) {
            if (i % 2 != 0) continue;
            const double angle = 2.0 * pi * i / dots;
            dot(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        }
        const double inner = radius * 0.58;
        for (int i = 0; i < dots; ++i) {
            if (i % 3 != 0) continue;
            const double angle = -2.0 * pi * i / dots + 0.2;
            dot(cx + std::cos(angle) * inner, cy + std::sin(angle) * inner);
        }
        for (int step = -radius * 4 / 5; step <= radius * 4 / 5; ++step) {
            if (((step / 4) & 1) != 0) continue;
            dot(cx + step, cy);
            dot(cx, cy + step);
        }
        for (int ray = 0; ray < 8; ++ray) {
            const double angle = ray * pi / 4.0 + pi / 8.0;
            for (int step = 1; step <= 3; ++step) {
                const double r = radius * (1.12 + step * 0.07);
                dot(cx + std::cos(angle) * r, cy + std::sin(angle) * r);
            }
        }
        DeleteObject(brush);
    }

    void paintDesktopIcons(HDC dc, const MenuLayout& layout) const {
        const int textH = textHeight(dc, smallFont_);
        for (int order = 0; order < kMenuItems; ++order) {
            const int i = order == kMenuItems - 1 ? menuIndex_ :
                          (order >= menuIndex_ ? order + 1 : order);
            const bool selected = i == menuIndex_ && screen_ == Screen::Menu;
            drawMenuIcon(dc, i, layout.iconTile[i], selected);
            const RECT& label = layout.iconLabel[i];
            const int center = (label.left + label.right) / 2;
            const int textW = textWidth(dc, smallFont_, kMenuLabels[i]);
            RECT caption{center - textW / 2 - px(4), label.top,
                         center + textW / 2 + px(4), label.bottom};
            fillColor(dc, caption, kBlack);
            drawTextCentered(dc, smallFont_, kMenuLabels[i], center,
                label.top + (label.bottom - label.top - textH) / 2, kWhite);
            if (selected) drawLine(dc, caption.left + px(4), caption.bottom,
                caption.right - px(4), caption.bottom);
        }
    }

    void paintAppWindow(HDC dc, const MenuLayout& layout, int index) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& window = layout.window;

        RECT shadow{window.left + px(7), window.top + px(7),
                    window.right + px(7), window.bottom + px(7)};
        fillDither(dc, shadow, 6);

        fillColor(dc, window, kBlack);
        strokeRect(dc, window, kWhite, 1);
        fillColor(dc, layout.windowTitle, kBlack);
        drawLine(dc, layout.windowTitle.left, layout.windowTitle.bottom - 1,
                 layout.windowTitle.right, layout.windowTitle.bottom - 1);

        const int titleTextH = textHeight(dc, bodyFont_);
        const int titleY = layout.windowTitle.top +
                           (layout.windowTitle.bottom -
                            layout.windowTitle.top - titleTextH) / 2;
        const int iconCell = std::max(1, px(1));
        const int iconPixels = 16 * iconCell;
        const int iconLeft = layout.windowTitle.left + px(8);
        const int iconTop = layout.windowTitle.top +
                            (layout.windowTitle.bottom -
                             layout.windowTitle.top - iconPixels) / 2;
        drawGlyph(dc, appIconRows(index), iconLeft, iconTop, iconCell, kWhite);
        drawTextAt(dc, bodyFont_, kMenuTitles[index],
                   iconLeft + iconPixels + px(8), titleY, kWhite);

        // Each app owns its close, maximize/restore and minimize controls.
        struct WindowButton {
            const RECT* rect;
            int glyph;
            MenuPart part;
        };
        const WindowButton buttons[3] = {
            {&layout.windowClose, 0, MenuPart::WindowClose},
            {&layout.windowMax, 1, MenuPart::WindowMax},
            {&layout.windowMin, 2, MenuPart::WindowMin},
        };
        for (const WindowButton& button : buttons) {
            const RECT& control = *button.rect;
            const bool active = hot(button.part);
            if (active) {
                fillColor(dc, control, kWhite);
            } else {
                strokeRect(dc, control, kWhite, 1);
            }
            const COLORREF ink = active ? kBlack : kWhite;
            if (button.glyph == 0) {
                drawLine(dc, control.left + px(3), control.top + px(3),
                         control.right - px(3), control.bottom - px(3), ink);
                drawLine(dc, control.right - px(3), control.top + px(3),
                         control.left + px(3), control.bottom - px(3), ink);
            } else if (button.glyph == 1) {
                RECT inner{control.left + px(3), control.top + px(3),
                           control.right - px(3), control.bottom - px(3)};
                strokeRect(dc, inner, ink, 1);
            } else {
                drawLine(dc, control.left + px(3),
                         (control.top + control.bottom) / 2,
                         control.right - px(3),
                         (control.top + control.bottom) / 2, ink);
            }
        }

    }

    void paintDesktopApp(HDC dc, int index) {
        const MenuLayout layout = appWindowLayout(index);
        paintAppWindow(dc, layout, index);
        const RECT client = contentClient(index);
        HDC content = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
        HGDIOBJ old = SelectObject(content, bitmap);
        SetBkMode(content, TRANSPARENT);
        fillColor(content, client, kBlack);
        if (index == 0) paintLibrary(content, client.right, client.bottom);
        else if (index == 1) paintPaintScreen(content, client.right, client.bottom);
        else if (index == 2) paintBrowser(content, client.right, client.bottom);
        else if (index == 3) snake_.draw(content, client.right, client.bottom, nowMs());
        else tentacle_.draw(content, client.right, client.bottom, nowMs());
        const RECT body = layout.windowBody;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc, body.left, body.top, body.right - body.left, body.bottom - body.top,
                   content, 0, 0, client.right, client.bottom, SRCCOPY);
        SelectObject(content, old);
        DeleteObject(bitmap);
        DeleteDC(content);
    }

    void paintTaskBar(HDC dc, const MenuLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& bar = layout.taskBar;
        fillColor(dc, bar, kBlack);
        drawLine(dc, bar.left, bar.top, bar.right, bar.top);

        const int textH = textHeight(dc, smallFont_);
        const int centerY = bar.top + (bar.bottom - bar.top - textH) / 2;
        const int logoCell = std::max(1, px(1));
        const int logoPixels = kLogoGrid * logoCell;
        const RECT& start = layout.startButton;
        strokeRect(dc, start, kWhite, 1);
        drawLogo(dc, (start.left + start.right - logoPixels) / 2,
                 start.top + (start.bottom - start.top - logoPixels) / 2,
                 logoCell, kWhite);
        if (hot(MenuPart::Start)) {
            drawDashedRect(dc, start, px(3), px(2), kWhite);
        }

        for (int i = 0; i < kAppCount; ++i) {
            if (!appWindows_[i].open) continue;
            const std::wstring label = kMenuLabels[i];
            const RECT& chip = layout.taskChip[i];
            if (i == activeApp()) {
                fillColor(dc, chip, kWhite);
                drawTextCentered(dc, smallFont_, label,
                                 (chip.left + chip.right) / 2, centerY, kBlack);
            } else {
                strokeRect(dc, chip, kWhite, 1);
                drawTextCentered(dc, smallFont_, label,
                                 (chip.left + chip.right) / 2, centerY, kWhite);
                if (hot(MenuPart::TaskChip, i)) {
                    drawDashedRect(dc, chip, px(2), px(2));
                }
            }
        }

        const RECT& signal = layout.signal;
        const int cy = (signal.top + signal.bottom) / 2;
        const int sx = signal.left + px(4);
        RECT speaker{sx, cy - px(3), sx + px(4), cy + px(3)};
        fillColor(dc, speaker, kWhite);
        drawLine(dc, sx + px(4), cy - px(3), sx + px(9), cy - px(7));
        drawLine(dc, sx + px(9), cy - px(7), sx + px(9), cy + px(7));
        drawLine(dc, sx + px(9), cy + px(7), sx + px(4), cy + px(3));
        if (audio_.muted()) {
            drawLine(dc, sx + px(13), cy - px(4), sx + px(20), cy + px(4));
            drawLine(dc, sx + px(20), cy - px(4), sx + px(13), cy + px(4));
        } else {
            drawLine(dc, sx + px(13), cy - px(4), sx + px(16), cy);
            drawLine(dc, sx + px(16), cy, sx + px(13), cy + px(4));
            drawLine(dc, sx + px(18), cy - px(7), sx + px(22), cy);
            drawLine(dc, sx + px(22), cy, sx + px(18), cy + px(7));
        }
        if (hot(MenuPart::Signal)) {
            drawDashedRect(dc, signal, px(2), px(2));
        }

        const std::wstring clock = clockText();
        const int clockWidth = textWidth(dc, smallFont_, clock);
        const int clockX = bar.right - px(10) - clockWidth;
        drawTextAt(dc, smallFont_, clock, clockX, centerY, kWhite);

        if (startMenuVisible_) paintStartMenu(dc, layout);
    }

    void paintStartMenu(HDC dc, const MenuLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        fillColor(dc, layout.startMenu, kBlack);
        strokeRect(dc, layout.startMenu, kWhite, 1);
        const int textH = textHeight(dc, smallFont_);
        for (int i = 0; i < kMenuItems; ++i) {
            const RECT& item = layout.startItem[i];
            const bool active = hot(MenuPart::StartItem, i);
            if (active) {
                fillColor(dc, item, kWhite);
                drawTextAt(dc, smallFont_, kMenuLabels[i], item.left + px(8),
                           item.top + (item.bottom - item.top - textH) / 2,
                           kBlack);
            } else {
                drawTextAt(dc, smallFont_, kMenuLabels[i], item.left + px(8),
                           item.top + (item.bottom - item.top - textH) / 2,
                           kWhite);
            }
        }
    }

    void paintBezel(HDC dc, const MenuLayout& layout, int, int) const {
        const RECT& outer = layout.monitor;
        const RECT& screen = layout.screen;
        fillColor(dc, RECT{outer.left, outer.top, outer.right, screen.top}, kBlack);
        fillColor(dc, RECT{outer.left, screen.bottom, outer.right, outer.bottom}, kBlack);
        fillColor(dc, RECT{outer.left, screen.top, screen.left, screen.bottom}, kBlack);
        fillColor(dc, RECT{screen.right, screen.top, outer.right, screen.bottom}, kBlack);
        strokeRect(dc, screen, kWhite, 1);
    }

    void drawHeader(HDC dc, int width, const std::wstring& title,
                    HFONT titleFont, int logoCell, int& ruleY,
                    int& titleY) {
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int logoPixels = logoCell * kLogoGrid;
        drawLogo(dc, margin, margin, logoCell);

        const int fontHeight = textHeight(dc, titleFont);
        const int textX = margin + logoPixels + MulDiv(22, scale, 96);
        titleY = margin + (logoPixels - fontHeight) / 2;
        drawTextAt(dc, titleFont, title, textX, titleY, kWhite);

        ruleY = margin + logoPixels + MulDiv(20, scale, 96);
        drawLine(dc, margin, ruleY, width - margin, ruleY);
    }

    void paint(HDC screen) {
        RECT client{};
        GetClientRect(hwnd_, &client);
        paintInto(screen, client.right, client.bottom);
    }

    void paintInto(HDC screen, int width, int height) {
        if (width <= 0 || height <= 0) return;

        HDC dc = CreateCompatibleDC(screen);
        // Snake's entire composed screen retains the binary arcade palette,
        // including shell fonts that normally use antialiasing.
        std::uint32_t* snakePixels = nullptr;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        HBITMAP bitmap = screen_ == Screen::Snake
            ? CreateDIBSection(screen, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&snakePixels), nullptr, 0)
            : CreateCompatibleBitmap(screen, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        SetBkMode(dc, TRANSPARENT);

        RECT all{0, 0, width, height};
        fillColor(dc, all, kBlack);

        ensureFonts();
        const bool breaking =
            screen_ == Screen::Crash || screen_ == Screen::Search ||
            screen_ == Screen::Corridor ||
            (screen_ == Screen::Library && phase_ != GamePhase::Play);
        if (gods_.active()) {
            gods_.draw(dc, width, height);
        } else if (breaking) {
            // The break leaves nothing of the shell behind: no ring pattern,
            // no desktop, no taskbar. The app has taken the whole screen.
            paintBreak(dc, width, height);
        } else {
            if (screen_ != Screen::Splash && !backgroundHidden_) {
                background_.draw(dc, width, height, nowMs());
            }
            if (screen_ == Screen::Splash) paintSplash(dc, width, height);
            else paintMenu(dc, width, height);
        }

        if (snakePixels) {
            GdiFlush();
            for (std::size_t i=0;i<static_cast<std::size_t>(width)*height;++i) {
                const auto c=snakePixels[i];
                const unsigned light=(c&255)+((c>>8)&255)+((c>>16)&255);
                snakePixels[i]=light>=384 ? 0x00ffffffu : 0;
            }
        }
        BitBlt(screen, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }

    void paintSplash(HDC dc, int width, int height) {
        const DotArt& art = seraphimArt();
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int titleHeight = textHeight(dc, titleFont_);
        const int titleBlock = titleHeight + MulDiv(42, scale, 96);
        const int maxArtHeight =
            std::max(1, height - margin * 2 - titleBlock - MulDiv(50, scale, 96));
        const int maxArtWidth = std::max(1, width - margin * 2);
        // Dots sit on a square pitch that need not be a whole number of pixels,
        // so the angel fills whatever room the window leaves for it.
        const double pitch = std::max(
            1.0, std::min(static_cast<double>(maxArtWidth) / std::max(1, art.cols),
                          static_cast<double>(maxArtHeight) / std::max(1, art.rows)));
        const int artWidth = static_cast<int>(std::lround(art.cols * pitch));
        const int artHeight = static_cast<int>(std::lround(art.rows * pitch));
        const int artLeft = (width - artWidth) / 2;
        const int groupHeight = artHeight + MulDiv(28, scale, 96) + titleBlock;
        const int artTop = std::max(margin, (height - groupHeight) / 2);
        const int titleY = artTop + artHeight + MulDiv(28, scale, 96);

        const ULONGLONG elapsed = nowMs() - splashStart_;
        std::size_t visible = art.order.size();
        if (elapsed < kSplashRevealMs) {
            visible = static_cast<std::size_t>(
                art.order.size() * elapsed / kSplashRevealMs);
        }
        if (visible > art.order.size()) visible = art.order.size();

        // A dot has to be at least one whole pixel: a one-pixel ellipse is
        // degenerate and GDI draws nothing at all, so small dots are squares.
        const int dotSize =
            std::max(1, static_cast<int>(std::lround(pitch * 0.62)));
        HBRUSH brush = CreateSolidBrush(kWhite);
        HPEN pen = CreatePen(PS_SOLID, 1, kWhite);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        for (std::size_t i = 0; i < visible; ++i) {
            const auto& dot = art.order[i];
            const int centerX =
                artLeft + static_cast<int>(std::lround((dot.second + 0.5) * pitch));
            const int centerY =
                artTop + static_cast<int>(std::lround((dot.first + 0.5) * pitch));
            if (dotSize >= 3) {
                const int left = centerX - dotSize / 2;
                const int top = centerY - dotSize / 2;
                Ellipse(dc, left, top, left + dotSize - 1, top + dotSize - 1);
            } else {
                RECT pixel{centerX - dotSize / 2, centerY - dotSize / 2,
                           centerX - dotSize / 2 + dotSize,
                           centerY - dotSize / 2 + dotSize};
                FillRect(dc, &pixel, brush);
            }
        }
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);

        // The angel arrives on its own and holds for a beat; only then does the
        // title start to sputter.
        const ULONGLONG flickerStart = kSplashRevealMs + kSplashTitleDelayMs;
        const ULONGLONG flickerEnd = flickerStart + titleFlickerMs();
        if (elapsed >= flickerStart) {
            const int lit = elapsed >= flickerEnd
                                ? 100
                                : litTitlePercent(elapsed - flickerStart);
            drawSplashTitle(dc, width, titleY, titleHeight, lit);
        }
    }

    void paintMenu(HDC dc, int width, int height) {
        const MenuLayout layout = menuLayout(width, height);
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& screen = layout.screen;

        // The desktop is a CRT: the ring wallpaper is combed with scanlines
        // before any of the shell is laid on top of it.
        if (screen.right > screen.left && screen.bottom > screen.top) {
            const int saved = SaveDC(dc);
            IntersectClipRect(dc, screen.left, screen.top, screen.right,
                              screen.bottom);
            const int spacing = std::max(2, px(4));
            HBRUSH scan = CreateSolidBrush(kBlack);
            for (int y = screen.top; y < screen.bottom; y += spacing) {
                RECT line{screen.left, y, screen.right, y + 1};
                FillRect(dc, &line, scan);
            }
            DeleteObject(scan);

            // The desktop dedication sits at the top right, cut straight out
            // of the wallpaper: inside its letterforms the moving ring pattern
            // reads inverted. Windows opened later cover it, as on a real
            // desktop.
            drawInvertedTextRight(dc, dedicationFont_, kDesktopDedication,
                                  layout.desktop.right - px(18),
                                  layout.desktop.top + px(16));

            gods_.drawSparks(dc, nowMs());

            paintDesktopIcons(dc, layout);
            for (int index : windowOrder_) {
                if (appWindows_[index].open && !appWindows_[index].minimized)
                    paintDesktopApp(dc, index);
            }
            paintTaskBar(dc, layout);
            RestoreDC(dc, saved);
        }

        paintBezel(dc, layout, width, height);
    }

    // ------------------------------------------------ bethelament art & layout
    RoomLayout roomLayout(int width, int height) const {
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int logoCell = std::max(2, MulDiv(4, scale, 96));
        const int headerBottom =
            margin + logoCell * kLogoGrid + MulDiv(18, scale, 96) + 1;
        const int topLimit = headerBottom + MulDiv(18, scale, 96);
        const int bottomLimit = height - MulDiv(48, scale, 96);
        const int availableWidth = width - margin * 2;
        const int availableHeight = std::max(1, bottomLimit - topLimit);
        int cell = std::min(availableWidth / kRoomCols,
                            availableHeight / kRoomRows);
        cell = std::max(6, cell);
        RoomLayout layout;
        layout.cell = cell;
        layout.left = (width - cell * kRoomCols) / 2;
        layout.top =
            topLimit + std::max(0, (availableHeight - cell * kRoomRows) / 2);
        return layout;
    }

    // Books standing on a shelf: white spines of uneven height, like the logo.
    void drawShelfBooks(HDC dc, int left, int top, int width, int height,
                        int seed) const {
        if (width <= 0 || height <= 0) return;
        HBRUSH brush = CreateSolidBrush(kWhite);
        const int unit = std::max(1, width / 20);
        int index = 0;
        for (int x = left; x + unit * 2 <= left + width; x += unit * 3) {
            const int variance = (seed * 37 + index * 53) % 45;
            const int bookHeight = std::max(1, height * (55 + variance) / 100);
            RECT book{x, top + height - bookHeight, x + unit * 2,
                      top + height};
            FillRect(dc, &book, brush);
            ++index;
        }
        DeleteObject(brush);
    }

    // A lit candle in the ossuary: a stick, a fluttering flame and a ring of
    // sparks. The flicker reads the clock, so a headless frame stays fixed.
    void drawCandle(HDC dc, int left, int top, int cell, int seed) const {
        const int cx = left + cell / 2;
        const int baseY = top + cell - std::max(2, cell / 10);
        const int stem = std::max(1, cell / 14);
        fillColor(dc, RECT{cx - stem, baseY - cell / 2, cx + stem, baseY},
                  kWhite);
        const double flicker = std::sin(nowMs() * 0.012 + seed * 2.1);
        const int sway = static_cast<int>(std::lround(flicker * cell * 0.04));
        const int flameW = std::max(2, cell / 8);
        const int flameH = std::max(3, cell / 5);
        const int flameTop = baseY - cell / 2 - flameH;
        fillColor(dc, RECT{cx - flameW / 2 + sway, flameTop,
                          cx + flameW / 2 + sway, flameTop + flameH},
                  kWhite);
        HBRUSH brush = CreateSolidBrush(kWhite);
        const int dot = std::max(1, cell / 40);
        for (int i = 0; i < 6; ++i) {
            const double angle = i * kPi / 3.0 + nowMs() * 0.0007 + seed;
            const int x = cx + sway + static_cast<int>(std::lround(
                std::cos(angle) * cell * 0.22));
            const int y = flameTop + flameH / 2 + static_cast<int>(std::lround(
                std::sin(angle) * cell * 0.16));
            RECT pixel{x, y, x + dot, y + dot};
            FillRect(dc, &pixel, brush);
        }
        DeleteObject(brush);
    }

    // A stand with a handle. The solid handle is where the lever stands; the
    // dotted ghost is where the carving wants it. Matching the two is the
    // puzzle, so the carving needs no arrow and no words.
    void drawLever(HDC dc, int left, int top, int cell, bool on,
                   bool target) const {
        const int cx = left + cell / 2;
        const int thick = std::max(2, cell / 18);
        const int baseY = top + cell - std::max(3, cell / 7);
        const int baseW = std::max(8, cell / 2);
        const int baseH = std::max(2, cell / 12);
        fillColor(dc, RECT{cx - baseW / 2, baseY - baseH, cx + baseW / 2, baseY},
                  kWhite);
        const int foot = std::max(2, cell / 20);
        fillColor(dc, RECT{cx - baseW / 2 - foot, baseY,
                          cx + baseW / 2 + foot, baseY + foot}, kWhite);
        const int pivotY =
            baseY - baseH - std::max(4, cell * 2 / 5);
        fillColor(dc, RECT{cx - thick / 2, pivotY,
                          cx + thick - thick / 2, baseY - baseH}, kWhite);

        auto endOf = [&](bool up, int& ex, int& ey) {
            if (up) {
                ex = cx;
                ey = std::max(top + 1, pivotY - std::max(3, cell * 3 / 10));
            } else {
                ex = cx + std::max(3, cell / 4);
                ey = pivotY + std::max(3, cell / 5);
            }
        };
        auto dot = [&](int x, int y) {
            const int size = std::max(1, thick);
            fillColor(dc, RECT{x - size / 2, y - size / 2,
                              x - size / 2 + size, y - size / 2 + size},
                      kWhite);
        };
        auto knob = [&](int x, int y, bool hollow) {
            const int radius = std::max(2, cell / 11);
            HPEN pen = CreatePen(PS_SOLID, std::max(1, thick / 2), kWhite);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(
                dc, hollow ? GetStockObject(NULL_BRUSH)
                           : GetStockObject(WHITE_BRUSH));
            Ellipse(dc, x - radius, y - radius, x + radius, y + radius);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        };

        // The carving: a dotted handle and a hollow knob in the wanted pose.
        int targetX = 0;
        int targetY = 0;
        endOf(target, targetX, targetY);
        const int steps = std::max(6, cell / 4);
        for (int i = 0; i <= steps; ++i) {
            if ((i / 2) % 2 != 0) continue;
            dot(cx + (targetX - cx) * i / steps,
                pivotY + (targetY - pivotY) * i / steps);
        }
        knob(targetX, targetY, true);

        // The lever itself, over the carving.
        int handleX = 0;
        int handleY = 0;
        endOf(on, handleX, handleY);
        HPEN pen = CreatePen(PS_SOLID, thick, kWhite);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, cx, pivotY, nullptr);
        LineTo(dc, handleX, handleY);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        knob(handleX, handleY, false);
        const int boss = std::max(1, cell / 16);
        fillColor(dc, RECT{cx - boss, pivotY - boss, cx + boss, pivotY + boss},
                  kWhite);
    }

    // A plate carved with its numeral: I, II or III as upright strokes, so it
    // stays legible at any cell size. A lit plate is reversed out.
    void drawPlate(HDC dc, int left, int top, int cell, int number,
                   bool lit) const {
        const int inset = std::max(3, cell / 5);
        const RECT plate{left + inset, top + inset, left + cell - inset,
                         top + cell - inset};
        fillColor(dc, plate, lit ? kWhite : kBlack);
        strokeRect(dc, plate, kWhite, 1);
        const int rule = std::max(1, cell / 18);
        if (plate.right - plate.left > rule * 8 &&
            plate.bottom - plate.top > rule * 8) {
            strokeRect(dc, RECT{plate.left + rule, plate.top + rule,
                                plate.right - rule, plate.bottom - rule},
                       kWhite, 1);
        }
        const int count = std::clamp(number, 0, 2) + 1;
        const int stroke = std::max(2, cell / 16);
        const int gap = std::max(2, cell / 9);
        const int barHeight = std::max(4, static_cast<int>((plate.bottom - plate.top) / 2));
        const int total = count * stroke + (count - 1) * gap;
        const int x0 = (plate.left + plate.right - total) / 2;
        const int y0 = (plate.top + plate.bottom - barHeight) / 2;
        const COLORREF ink = lit ? kBlack : kWhite;
        for (int i = 0; i < count; ++i) {
            const int x = x0 + i * (stroke + gap);
            fillColor(dc, RECT{x, y0, x + stroke, y0 + barHeight}, ink);
        }
    }

    void drawKeyTile(HDC dc, int left, int top, int cell) const {
        const int cx = left + cell / 2;
        const int cy = top + cell / 2;
        const int radius = std::max(3, cell / 8);
        const int thickness = std::max(1, cell / 26);
        HPEN pen = CreatePen(PS_SOLID, thickness, kWhite);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        // Stem and two teeth, pointing right.
        fillColor(dc, RECT{cx + radius, cy - thickness / 2, cx + radius * 3,
                          cy - thickness / 2 + std::max(1, thickness)},
                  kWhite);
        fillColor(dc, RECT{cx + radius * 2, cy, cx + radius * 2 + thickness,
                          cy + radius}, kWhite);
        fillColor(dc, RECT{cx + radius * 3 - thickness, cy,
                          cx + radius * 3, cy + radius * 2 / 3}, kWhite);
    }

    // A grave niche: an arched recess with two lit eye-dots inside.
    void drawGrave(HDC dc, int left, int top, int cell) const {
        const int pad = std::max(2, cell / 8);
        const RECT niche{left + pad, top + pad, left + cell - pad,
                         top + cell - pad};
        fillColor(dc, niche, kBlack);
        strokeRect(dc, niche, kWhite, 1);
        const int third = niche.top + (niche.bottom - niche.top) / 3;
        drawLine(dc, niche.left + 1, third, niche.right - 1, third);
        const int cx = (niche.left + niche.right) / 2;
        const int cy = (third + niche.bottom) / 2;
        const int dot = std::max(1, cell / 22);
        fillColor(dc, RECT{cx - cell / 8, cy - dot / 2, cx - cell / 8 + dot,
                          cy - dot / 2 + dot}, kWhite);
        fillColor(dc, RECT{cx + cell / 8 - dot, cy - dot / 2,
                          cx + cell / 8, cy - dot / 2 + dot}, kWhite);
        drawLine(dc, cx - dot * 2, cy + cell / 8, cx + dot * 2, cy + cell / 8);
    }

    // A cloister pillar: base, shaft and capital.
    void drawPillar(HDC dc, int left, int top, int cell) const {
        const int shaftLeft = left + cell / 3;
        const int shaftRight = left + cell * 2 / 3;
        const int baseTop = top + cell - cell / 6;
        const int capTop = top + cell / 5;
        drawLine(dc, shaftLeft, capTop + cell / 10, shaftLeft, baseTop);
        drawLine(dc, shaftRight, capTop + cell / 10, shaftRight, baseTop);
        fillColor(dc, RECT{shaftLeft - cell / 10, capTop, shaftRight + cell / 10,
                          capTop + cell / 10}, kWhite);
        fillColor(dc, RECT{shaftLeft - cell / 10, baseTop,
                          shaftRight + cell / 10, baseTop + cell / 8}, kWhite);
        drawLine(dc, (shaftLeft + shaftRight) / 2, capTop + cell / 6,
                 (shaftLeft + shaftRight) / 2, baseTop - cell / 12);
    }

    // The breach: a slit of nothing, its edges eaten by dither, with a hooded
    // shape almost readable inside it.
    void drawBreach(HDC dc, int left, int top, int cell) const {
        fillDither(dc, RECT{left + cell / 5, top + cell / 6, left + cell * 4 / 5,
                            top + cell - cell / 7}, 4);
        fillColor(dc, RECT{left + cell / 3, top + cell / 3, left + cell * 2 / 3,
                          top + cell - cell / 4}, kBlack);
        for (int i = 0; i < 6; ++i) {
            const int x = left + cell / 4 + (i * cell * 3) / 20;
            const int length = cell / 8 + ((i * 37) % (cell / 3));
            drawLine(dc, x, top + cell / 6, x, top + cell / 6 + length);
        }
        drawGhostFigure(dc, left + cell / 2, top + cell - cell / 5,
                        std::max(1, cell / 22), kFaceDown, kStandPose, 3, 3);
    }

    // A spirit: the same pilgrim silhouette, thinned to a stipple so the room
    // shows through it. Used for the ghosts in the breach and the crash.
    void drawGhostFigure(HDC dc, int centerX, int feetY, int pixelScale,
                         int facing, int pose, int seed, int density) const {
        if (pixelScale < 1) pixelScale = 1;
        if (density < 1) density = 1;
        const int left = centerX - kWalkerW * pixelScale / 2;
        const int top = feetY - kWalkerH * pixelScale;
        HBRUSH brush = CreateSolidBrush(kWhite);
        for (int sy = 0; sy < kWalkerH; ++sy) {
            for (int sx = 0; sx < kWalkerW; ++sx) {
                if (!walkerSourcePixel(sx, sy, facing, pose)) continue;
                if (density > 1 &&
                    ((sx * 5 + sy * 7 + seed * 11) % density) != 0) {
                    continue;
                }
                RECT block{left + sx * pixelScale, top + sy * pixelScale,
                           left + (sx + 1) * pixelScale,
                           top + (sy + 1) * pixelScale};
                FillRect(dc, &block, brush);
            }
        }
        DeleteObject(brush);
    }

    // A door sits in the wall band. Locked, it is a leaf with its lock carved
    // on it; open, it is a throat of black with light on the threshold.
    void drawDoorway(HDC dc, const RoomLayout& room, const RoomDoor& door) const {
        const int cell = room.cell;
        const int left = room.left + door.x * cell;
        const int top = room.top + door.y * cell;
        const int frame = std::max(2, cell / 10);
        const RECT opening{left + frame, top + cell / 6, left + cell - frame,
                           top + cell - frame};
        if (doorUnlocked(door)) {
            fillColor(dc, opening, kBlack);
            strokeRect(dc, opening, kWhite, 1);
            const int threshold = std::max(2, cell / 12);
            fillColor(dc, RECT{opening.left + 1,
                              opening.bottom - threshold,
                              opening.right - 1, opening.bottom}, kWhite);
            // Light spilling down onto the floor of this room.
            for (int i = 1; i <= 3; ++i) {
                const int spread = i * cell / 9;
                drawLine(dc, (opening.left + opening.right) / 2 - spread,
                         opening.bottom + i * cell / 12,
                         (opening.left + opening.right) / 2 + spread,
                         opening.bottom + i * cell / 12);
            }
            return;
        }
        fillColor(dc, opening, kBlack);
        strokeRect(dc, opening, kWhite, 1);
        const int middle = (opening.left + opening.right) / 2;
        drawLine(dc, middle, opening.top + 1, middle, opening.bottom - 1);
        if (door.lock == kDoorKey) {
            const int cx = middle;
            const int cy = (opening.top + opening.bottom) / 2;
            const int radius = std::max(1, cell / 16);
            HPEN pen = CreatePen(PS_SOLID, 1, kWhite);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
            drawLine(dc, cx, cy + radius, cx, cy + radius * 3);
        } else if (door.lock == kDoorLevers) {
            for (int i = -1; i <= 1; ++i) {
                const int x = middle + i * cell / 7;
                drawLine(dc, x, (opening.top + opening.bottom) / 2 - cell / 12,
                         x, (opening.top + opening.bottom) / 2 + cell / 12);
            }
        } else if (door.lock == kDoorPlates) {
            for (int i = -1; i <= 1; ++i) {
                const int x = middle + i * cell / 8;
                strokeRect(dc, RECT{x - cell / 24,
                                    (opening.top + opening.bottom) / 2 - cell / 24,
                                    x + cell / 24,
                                    (opening.top + opening.bottom) / 2 + cell / 24},
                           kWhite, 1);
            }
        }
    }

    void drawRoom(HDC dc, const RoomLayout& room) const {
        const int cell = room.cell;
        const int style = kRooms[roomId_].style;
        const int width = cell * kRoomCols;
        const int height = cell * kRoomRows;
        const RECT roomRect{room.left, room.top, room.left + width,
                            room.top + height};
        fillColor(dc, roomRect, kBlack);
        strokeRect(dc, roomRect, kWhite, 1);

        // Inner edge of the wall band, one tile in from the outer wall.
        const int innerLeft = room.left + cell;
        const int innerTop = room.top + cell;
        const int innerRight = room.left + width - cell;
        const int innerBottom = room.top + height - cell;
        drawLine(dc, innerLeft, innerTop, room.left + width - 1, innerTop);
        drawLine(dc, innerLeft, innerTop, innerLeft, innerBottom);
        drawLine(dc, innerRight, innerTop, innerRight, innerBottom);
        drawLine(dc, innerLeft, innerBottom, room.left + width - 1, innerBottom);

        // A stippled floor: one dot at each tile corner, so the grid reads.
        const int dot = std::max(1, cell / 48);
        HBRUSH dotBrush = CreateSolidBrush(kWhite);
        for (int row = 2; row < kRoomRows - 1; ++row) {
            for (int col = 2; col < kRoomCols - 1; ++col) {
                const int x = room.left + col * cell;
                const int y = room.top + row * cell;
                RECT pixel{x - dot / 2, y - dot / 2, x - dot / 2 + dot,
                           y - dot / 2 + dot};
                FillRect(dc, &pixel, dotBrush);
            }
        }
        DeleteObject(dotBrush);

        const int pad = std::max(2, cell / 14);
        if (style == kStyleLibrary) {
            // The rug, laid over the middle of the floor.
            const int rugLeft = room.left + 5 * cell;
            const int rugTop = room.top + 3 * cell;
            const int rugInset = std::max(2, cell / 6);
            const RECT rug{rugLeft + rugInset, rugTop + rugInset,
                           rugLeft + 3 * cell - rugInset,
                           rugTop + 3 * cell - rugInset};
            strokeRect(dc, rug, kWhite, 1);
            const int border = std::max(2, cell / 8);
            const RECT rugInner{rug.left + border, rug.top + border,
                                rug.right - border, rug.bottom - border};
            if (rugInner.right > rugInner.left &&
                rugInner.bottom > rugInner.top) {
                strokeRect(dc, rugInner, kWhite, 1);
            }
        } else if (style == kStyleCloister) {
            drawSigil(dc, room.left + width / 2, room.top + height / 2,
                      std::max(8, cell * 13 / 10));
        } else if (style == kStyleOssuary) {
            // A slab over the middle of the floor, and lines between graves.
            const RECT slab{room.left + 5 * cell, room.top + 4 * cell,
                            room.left + 8 * cell, room.top + 5 * cell};
            strokeRect(dc, slab, kWhite, 1);
            drawLine(dc, slab.left + pad, (slab.top + slab.bottom) / 2,
                     slab.right - pad, (slab.top + slab.bottom) / 2);
        } else {
            // The breach: the floor itself cannot hold still.
            for (int i = 0; i < 16; ++i) {
                const int x = room.left + cell + ((i * 149) % (width - 2 * cell));
                const int y = room.top + cell + ((i * 83) % (height - 2 * cell));
                const int w = cell / 2 + (i * 11) % (cell / 2 + 1);
                const int h = std::max(2, cell / 10) + (i % 3) * (cell / 8);
                fillDither(dc, RECT{x, y, x + w, y + h}, 3);
                if (i % 4 == 0) {
                    // A tear that runs the other way.
                    fillDither(dc, RECT{x + w / 3, y - cell / 3,
                                        x + w / 3 + std::max(2, cell / 12),
                                        y + h + cell / 2}, 2);
                }
            }
        }

        // The top wall, decorated by the room's own style.
        if (style == kStyleLibrary) {
            for (int col = 1; col < kRoomCols; ++col) {
                const int x = room.left + col * cell;
                drawLine(dc, x, room.top + cell / 5, x, room.top + cell);
            }
            for (int col = 1; col < kRoomCols - 1; ++col) {
                if (doorAt(roomId_, col, 0)) continue;
                drawShelfBooks(dc, room.left + col * cell + pad,
                               room.top + pad * 2, cell - pad * 2,
                               cell - pad * 2, col);
            }
        } else if (style == kStyleCloister) {
            for (int col = 1; col < kRoomCols - 1; ++col) {
                const int x = room.left + col * cell;
                drawLine(dc, x, room.top + cell / 5, x, room.top + cell);
                if (doorAt(roomId_, col, 0)) continue;
                const int spring = room.top + cell * 2 / 5;
                drawLine(dc, x + pad, room.top + cell - pad, x + pad, spring);
                drawLine(dc, x + cell - pad, room.top + cell - pad,
                         x + cell - pad, spring);
                drawLine(dc, x + pad, spring, x + cell / 2, room.top + pad);
                drawLine(dc, x + cell - pad, spring, x + cell / 2,
                         room.top + pad);
            }
        } else if (style == kStyleOssuary) {
            for (int col = 1; col < kRoomCols - 1; ++col) {
                if (doorAt(roomId_, col, 0)) continue;
                const int x = room.left + col * cell;
                const RECT niche{x + pad, room.top + pad, x + cell - pad,
                                 room.top + cell - pad};
                strokeRect(dc, niche, kWhite, 1);
                const int third = niche.top + (niche.bottom - niche.top) / 3;
                drawLine(dc, niche.left + 1, third, niche.right - 1, third);
            }
        } else {
            for (int col = 1; col < kRoomCols - 1; ++col) {
                const int x = room.left + col * cell;
                if ((col * 7) % 5 == 0) continue;
                const int y = room.top + cell / 4 + (col * 13) % (cell / 3);
                drawLine(dc, x, y, x + cell / 2, y);
                if (col % 3 == 0) {
                    drawLine(dc, x + cell / 2, y, x + cell / 2,
                             y + cell / 3);
                }
            }
        }

        // Solid tiles: shelves, pillars, tombs, candles and the machinery of
        // the puzzles.
        for (int row = 0; row < kRoomRows; ++row) {
            for (int col = 0; col < kRoomCols; ++col) {
                const char tile = roomTile(roomId_, col, row);
                const int left = room.left + col * cell;
                const int top = room.top + row * cell;
                if (tile == L'B') {
                    if (style == kStyleCloister) {
                        drawPillar(dc, left, top, cell);
                        continue;
                    }
                    if (style == kStyleOssuary) {
                        drawGrave(dc, left, top, cell);
                        continue;
                    }
                    const int frame = std::max(2, cell / 10);
                    const int x0 = left + frame;
                    const int y0 = top + frame;
                    const int x1 = left + cell - frame;
                    const int y1 = top + cell - frame;
                    strokeRect(dc, RECT{x0, y0, x1, y1}, kWhite, 1);
                    if (style == kStyleBreach && (col + row) % 2 == 0) {
                        // A shelf that has forgotten how to be a shelf.
                        drawLine(dc, x0 + 1, (y0 + y1) / 2, x1 - 1,
                                 (y0 + y1) / 2 - cell / 6);
                        drawShelfBooks(dc, x0 + 2, y0 + cell / 10, x1 - x0 - 4,
                                       cell / 3, col * 3 + row);
                        continue;
                    }
                    const int middle = (y0 + y1) / 2;
                    drawLine(dc, x0 + 1, middle, x1 - 1, middle);
                    const int gap = std::max(1, cell / 20);
                    const int bookTop = y0 + std::max(2, cell / 12);
                    const int lowerTop = middle + std::max(2, cell / 12);
                    drawShelfBooks(dc, x0 + 2, bookTop, x1 - x0 - 4,
                                   middle - bookTop - gap,
                                   row * kRoomCols + col);
                    drawShelfBooks(dc, x0 + 2, lowerTop, x1 - x0 - 4,
                                   y1 - lowerTop - gap, col * 7 + row);
                } else if (tile == L'G') {
                    drawGrave(dc, left, top, cell);
                } else if (tile == L'C') {
                    drawCandle(dc, left, top, cell, col * 5 + row);
                } else if (tile == L'L') {
                    const int index = leverIndexAt(col, row);
                    drawLever(dc, left, top, cell,
                              index >= 0 && index < kLeverCount &&
                                  levers_[index],
                              index >= 0 && index < kLeverCount &&
                                  kLeverTarget[index]);
                } else if (tile == L'K') {
                    if (!keyTaken_) drawKeyTile(dc, left, top, cell);
                } else if (tile >= L'1' && tile <= L'3') {
                    const int number = tile - L'1';
                    drawPlate(dc, left, top, cell, number,
                              plateSolved_ || number < plateStep_);
                } else if (tile == L'O') {
                    drawBreach(dc, left, top, cell);
                } else if (tile == L'~') {
                    fillDither(dc, RECT{left + cell / 3, top + cell / 3,
                                        left + cell - cell / 3,
                                        top + cell - cell / 3}, 3);
                    const int phase = static_cast<int>(nowMs() / 140);
                    for (int i = 0; i < 5; ++i) {
                        const int x = left + ((i * 37 + phase * 11) % cell);
                        const int y = top + ((i * 53 + phase * 7) % cell);
                        fillColor(dc, RECT{x, y, x + std::max(1, cell / 28),
                                          y + std::max(1, cell / 28)}, kWhite);
                    }
                }
            }
        }

        // The doors sit in the wall band, over everything underneath.
        for (const RoomDoor& door : kRoomDoors[roomId_]) {
            if (door.x < 0) continue;
            drawDoorway(dc, room, door);
        }

        // In the breach, the tear climbs out of its tile and into the wall.
        if (style == kStyleBreach) {
            for (int row = 0; row < kRoomRows; ++row) {
                for (int col = 0; col < kRoomCols; ++col) {
                    if (roomTile(roomId_, col, row) != L'O') continue;
                    const int x = room.left + col * cell + cell / 2;
                    const int topY = room.top + cell / 5;
                    int y = room.top + row * cell + cell / 4;
                    int step = 0;
                    while (y > topY) {
                        const int bend = ((step * 37) % 7) - 3;
                        const int bendX = x + bend * cell / 20;
                        drawLine(dc, bendX, y, bendX, y - cell / 5);
                        if (step % 3 == 1) {
                            drawLine(dc, bendX, y, bendX + cell / 6, y - cell / 8);
                        }
                        y -= cell / 5;
                        ++step;
                    }
                }
            }
        }

        // In the breach, the room keeps rendering the pilgrim in places the
        // pilgrim is not. They are thinner than he is and never quite line up.
        if (style == kStyleBreach) {
            const int scale = std::max(1, cell / 18);
            const int pulse = static_cast<int>((nowMs() / 170) % 6);
            const int drift = static_cast<int>(std::lround(
                std::sin(nowMs() * 0.0013) * cell * 0.18));
            struct GhostSpot {
                int x;  // half-cells from the room's left edge
                int y;  // half-cells from the room's top edge
                int facing;
                int seed;
                int density;
            };
            static constexpr GhostSpot kGhosts[5] = {
                {5, 6, kFaceDown, 1, 2},   {21, 12, kFaceLeft, 2, 2},
                {13, 4, kFaceDown, 3, 2},  {9, 14, kFaceRight, 4, 3},
                {17, 10, kFaceDown, 5, 3},
            };
            for (const GhostSpot& ghost : kGhosts) {
                const int x = room.left + cell * ghost.x / 2 +
                              (ghost.seed % 2 == 0 ? drift : -drift);
                const int y = room.top + cell * ghost.y / 2;
                const int density =
                    ghost.density + ((pulse + ghost.seed) % 3 == 0 ? 1 : 0);
                drawGhostFigure(dc, x, y, scale, ghost.facing, kStandPose,
                                ghost.seed, density);
                // A second, thinner copy one step behind.
                drawGhostFigure(dc, x + scale * 2, y - scale, scale,
                                ghost.facing, kStandPose, ghost.seed + 7,
                                density + 2);
            }
        }
    }

    struct WalkerGeometry {
        int scale = 1;
        int spriteWidth = 0;
        int spriteHeight = 0;
        int feetX = 0;  // room-local
        int feetY = 0;  // room-local
        int left = 0;   // room-local
        int top = 0;    // room-local
    };

    // Where the pilgrim's sprite sits inside the room, in room-local pixels.
    WalkerGeometry walkerGeometry(const RoomLayout& room) const {
        WalkerGeometry geometry;
        geometry.scale = std::max(1, room.cell / 16);
        const double col =
            walker_.tileX +
            (walker_.moving ? kDirX[walker_.dir] * walker_.progress : 0.0);
        const double row =
            walker_.tileY +
            (walker_.moving ? kDirY[walker_.dir] * walker_.progress : 0.0);
        geometry.spriteWidth = kWalkerW * geometry.scale;
        geometry.spriteHeight = kWalkerH * geometry.scale;
        geometry.feetX =
            static_cast<int>(std::lround((col + 0.5) * room.cell));
        geometry.feetY =
            static_cast<int>(std::lround((row + 1.0) * room.cell)) -
            std::max(1, room.cell / 12);
        geometry.left = geometry.feetX - geometry.spriteWidth / 2;
        geometry.top = geometry.feetY - geometry.spriteHeight;
        return geometry;
    }

    // ------------------------------------------------------- pilgrim light
    // The sprite is a binary silhouette, so softness has to be built out of
    // black-and-white dots. While a step is in flight the body leaves a short
    // smear along the single axis it is walking on (never across it) and sits
    // in a dithered halo; when it rests, the smear is gone and only a quiet
    // glow remains. Order dithering, the same trick as the ring pattern, keeps
    // every pixel inside the two-colour palette.
    static bool walkerSourcePixel(int col, int row, int facing, int pose) {
        if (col < 0 || col >= kWalkerW || row < 0 || row >= kWalkerH) {
            return false;
        }
        const wchar_t* const* head = kWalkerHeadDown;
        if (facing == kFaceUp) {
            head = kWalkerHeadUp;
        } else if (facing != kFaceDown) {
            head = kWalkerHeadSide;
        }
        const wchar_t* const* body = kWalkerBodyStand;
        if (pose == 0) {
            body = kWalkerBodyStepA;
        } else if (pose == 2) {
            body = kWalkerBodyStepB;
        }
        const wchar_t* line = row < kWalkerHeadRows
                                  ? head[row]
                                  : body[row - kWalkerHeadRows];
        const int source = facing == kFaceRight ? kWalkerW - 1 - col : col;
        return line[source] == L'#';
    }

    // One separable box pass, run twice, turns the silhouette into the soft
    // light that reaches past its edge. Zero packed at the border because the
    // light field is padded by more than the blur can travel.
    static void boxBlur(std::vector<float>& field, std::vector<float>& scratch,
                        int width, int height, int radius) {
        if (radius <= 0 || width <= 0 || height <= 0) return;
        const float norm = 1.0f / static_cast<float>(2 * radius + 1);
        for (int y = 0; y < height; ++y) {
            const float* source =
                field.data() + static_cast<std::size_t>(y) * width;
            float* target = scratch.data() + static_cast<std::size_t>(y) * width;
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                sum += source[std::clamp(k, 0, width - 1)];
            }
            for (int x = 0; x < width; ++x) {
                target[x] = sum * norm;
                sum += source[std::clamp(x + radius + 1, 0, width - 1)] -
                       source[std::clamp(x - radius, 0, width - 1)];
            }
        }
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                const int y = std::clamp(k, 0, height - 1);
                sum += scratch[static_cast<std::size_t>(y) * width + x];
            }
            for (int y = 0; y < height; ++y) {
                field[static_cast<std::size_t>(y) * width + x] = sum * norm;
                const int add = std::clamp(y + radius + 1, 0, height - 1);
                const int drop = std::clamp(y - radius, 0, height - 1);
                sum += scratch[static_cast<std::size_t>(add) * width + x] -
                       scratch[static_cast<std::size_t>(drop) * width + x];
            }
        }
    }

    bool ensureWalkerSurface(int width, int height) {
        if (width <= 0 || height <= 0) return false;
        if (walkerDc_ && width == walkerWidth_ && height == walkerHeight_) {
            return true;
        }
        releaseWalkerSurface();

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;  // top-down rows
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        walkerBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits,
                                         nullptr, 0);
        if (!walkerBitmap_ || !bits) {
            releaseWalkerSurface();
            return false;
        }
        walkerPixels_ = static_cast<std::uint32_t*>(bits);
        walkerDc_ = CreateCompatibleDC(nullptr);
        if (!walkerDc_) {
            releaseWalkerSurface();
            return false;
        }
        walkerOldBitmap_ = SelectObject(walkerDc_, walkerBitmap_);
        walkerWidth_ = width;
        walkerHeight_ = height;
        const std::size_t count = static_cast<std::size_t>(width) * height;
        walkerLight_.assign(count, 0.0f);
        walkerTrail_.assign(count, 0.0f);
        walkerGlow_.assign(count, 0.0f);
        walkerScratch_.assign(count, 0.0f);
        return true;
    }

    void releaseWalkerSurface() {
        walkerLight_.clear();
        walkerTrail_.clear();
        walkerGlow_.clear();
        walkerScratch_.clear();
        if (walkerDc_) {
            if (walkerOldBitmap_) SelectObject(walkerDc_, walkerOldBitmap_);
            DeleteDC(walkerDc_);
        }
        if (walkerBitmap_) DeleteObject(walkerBitmap_);
        walkerDc_ = nullptr;
        walkerBitmap_ = nullptr;
        walkerOldBitmap_ = nullptr;
        walkerPixels_ = nullptr;
        walkerWidth_ = 0;
        walkerHeight_ = 0;
    }

    // ------------------------------------------------------------ room lens
    // The room is drawn to an offscreen surface so the space around the
    // pilgrim can be bent like a lens: magnified, turned slightly and combed
    // by a slow ripple. Sampling stays nearest-neighbour, so every pixel is
    // still pure black or pure white.
    bool ensureRoomSurface(int width, int height) {
        if (width <= 0 || height <= 0) return false;
        if (roomDc_ && width == roomWidth_ && height == roomHeight_) {
            return true;
        }
        releaseRoomSurface();

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;  // top-down rows
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        roomBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits,
                                       nullptr, 0);
        if (!roomBitmap_ || !bits) {
            releaseRoomSurface();
            return false;
        }
        roomPixels_ = static_cast<std::uint32_t*>(bits);
        roomDc_ = CreateCompatibleDC(nullptr);
        if (!roomDc_) {
            releaseRoomSurface();
            return false;
        }
        roomOldBitmap_ = SelectObject(roomDc_, roomBitmap_);
        roomWidth_ = width;
        roomHeight_ = height;
        roomSource_.reserve(static_cast<std::size_t>(width) * height);
        return true;
    }

    void releaseRoomSurface() {
        roomSource_.clear();
        roomSource_.shrink_to_fit();
        if (roomDc_) {
            if (roomOldBitmap_) SelectObject(roomDc_, roomOldBitmap_);
            DeleteDC(roomDc_);
        }
        if (roomBitmap_) DeleteObject(roomBitmap_);
        roomDc_ = nullptr;
        roomBitmap_ = nullptr;
        roomOldBitmap_ = nullptr;
        roomPixels_ = nullptr;
        roomWidth_ = 0;
        roomHeight_ = 0;
    }

    // A full-frame copy of the library, so the crash can tear it into bands.
    bool ensureBreakSurface(int width, int height) {
        if (width <= 0 || height <= 0) return false;
        if (breakDc_ && width == breakWidth_ && height == breakHeight_) {
            return true;
        }
        releaseBreakSurface();
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        breakBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits,
                                        nullptr, 0);
        if (!breakBitmap_ || !bits) {
            releaseBreakSurface();
            return false;
        }
        breakPixels_ = static_cast<std::uint32_t*>(bits);
        breakDc_ = CreateCompatibleDC(nullptr);
        if (!breakDc_) {
            releaseBreakSurface();
            return false;
        }
        breakOldBitmap_ = SelectObject(breakDc_, breakBitmap_);
        breakWidth_ = width;
        breakHeight_ = height;
        return true;
    }

    void releaseBreakSurface() {
        if (breakDc_) {
            if (breakOldBitmap_) SelectObject(breakDc_, breakOldBitmap_);
            DeleteDC(breakDc_);
        }
        if (breakBitmap_) DeleteObject(breakBitmap_);
        breakDc_ = nullptr;
        breakBitmap_ = nullptr;
        breakOldBitmap_ = nullptr;
        breakPixels_ = nullptr;
        breakWidth_ = 0;
        breakHeight_ = 0;
    }

    // The corridor's pixel surface: walls and ghosts are written straight in.
    bool ensureWorldSurface(int width, int height) {
        if (width <= 0 || height <= 0) return false;
        if (worldDc_ && width == worldWidth_ && height == worldHeight_) {
            return true;
        }
        releaseWorldSurface();
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        worldBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits,
                                        nullptr, 0);
        if (!worldBitmap_ || !bits) {
            releaseWorldSurface();
            return false;
        }
        worldPixels_ = static_cast<std::uint32_t*>(bits);
        worldDc_ = CreateCompatibleDC(nullptr);
        if (!worldDc_) {
            releaseWorldSurface();
            return false;
        }
        worldOldBitmap_ = SelectObject(worldDc_, worldBitmap_);
        worldWidth_ = width;
        worldHeight_ = height;
        corridorDepth_.assign(static_cast<std::size_t>(width), 0.0);
        return true;
    }

    void releaseWorldSurface() {
        corridorDepth_.clear();
        corridorDepth_.shrink_to_fit();
        if (worldDc_) {
            if (worldOldBitmap_) SelectObject(worldDc_, worldOldBitmap_);
            DeleteDC(worldDc_);
        }
        if (worldBitmap_) DeleteObject(worldBitmap_);
        worldDc_ = nullptr;
        worldBitmap_ = nullptr;
        worldOldBitmap_ = nullptr;
        worldPixels_ = nullptr;
        worldWidth_ = 0;
        worldHeight_ = 0;
    }

    void bendRoom(int centerX, int centerY, int radius, double strength) {
        if (radius <= 2 || strength <= 0.0) return;
        const int margin = std::max(2, radius / 4);
        const int x0 = std::max(0, centerX - radius - margin);
        const int y0 = std::max(0, centerY - radius - margin);
        const int x1 = std::min(roomWidth_, centerX + radius + margin + 1);
        const int y1 = std::min(roomHeight_, centerY + radius + margin + 1);
        const int boxWidth = x1 - x0;
        const int boxHeight = y1 - y0;
        if (boxWidth <= 0 || boxHeight <= 0) return;

        roomSource_.resize(static_cast<std::size_t>(boxWidth) * boxHeight);
        for (int y = 0; y < boxHeight; ++y) {
            std::memcpy(
                roomSource_.data() + static_cast<std::size_t>(y) * boxWidth,
                roomPixels_ + static_cast<std::size_t>(y0 + y) * roomWidth_ +
                    x0,
                static_cast<std::size_t>(boxWidth) * sizeof(std::uint32_t));
        }

        const double time = static_cast<double>(nowMs()) * 0.001;
        const double turn = 0.22 * strength * std::sin(time * 0.7);

        // The ripple depends only on the normalised distance, so build it once
        // per frame instead of calling sine for every pixel.
        constexpr int kRippleSteps = 256;
        std::array<float, kRippleSteps + 1> ripple{};
        for (int i = 0; i <= kRippleSteps; ++i) {
            ripple[i] = static_cast<float>(
                std::sin(static_cast<double>(i) / kRippleSteps * 9.0 -
                         time * 3.4));
        }

        const double maxRadius = static_cast<double>(radius);
        // The mapping for one point, shared by the four corners of a pixel.
        auto bendPoint = [&](double px, double py, double& outX,
                             double& outY) {
            const double dx = px - centerX;
            const double dy = py - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);
            const double t = std::min(1.0, distance / maxRadius);
            const double envelope = (1.0 - t * t) * (1.0 - t * t);
            const double pull = 1.0 - 0.42 * envelope * strength;
            const double bend = 0.10 * maxRadius * envelope * strength *
                                ripple[static_cast<int>(t * kRippleSteps)];
            const double angle = turn * envelope;
            const double cosTurn = 1.0 - 0.5 * angle * angle;
            const double sinTurn = angle;
            const double rx = dx * cosTurn - dy * sinTurn;
            const double ry = dx * sinTurn + dy * cosTurn;
            outX = centerX + rx * pull;
            outY = centerY + ry * pull;
            if (distance > 0.001) {
                outX += dx / distance * bend;
                outY += dy / distance * bend;
            }
        };

        for (int y = y0; y < y1; ++y) {
            const double dy = (static_cast<double>(y) + 0.5) - centerY;
            std::uint32_t* const line =
                roomPixels_ + static_cast<std::size_t>(y) * roomWidth_;
            for (int x = x0; x < x1; ++x) {
                const double dx = (static_cast<double>(x) + 0.5) - centerX;
                const double distance = std::sqrt(dx * dx + dy * dy);
                if (distance >= maxRadius) continue;

                // Every destination pixel covers a small patch of the room.
                // Collect that patch instead of picking one sample, so a
                // one-pixel shelf line cannot fall between two pixels and
                // break into dashes as the lens bends it.
                double ax = 0.0;
                double ay = 0.0;
                double bx = 0.0;
                double by = 0.0;
                double cx = 0.0;
                double cy = 0.0;
                double ex = 0.0;
                double ey = 0.0;
                bendPoint(x - 0.5, y - 0.5, ax, ay);
                bendPoint(x + 0.5, y - 0.5, bx, by);
                bendPoint(x - 0.5, y + 0.5, cx, cy);
                bendPoint(x + 0.5, y + 0.5, ex, ey);
                const double minX =
                    std::min(std::min(ax, bx), std::min(cx, ex));
                const double maxX =
                    std::max(std::max(ax, bx), std::max(cx, ex));
                const double minY =
                    std::min(std::min(ay, by), std::min(cy, ey));
                const double maxY =
                    std::max(std::max(ay, by), std::max(cy, ey));
                // Source pixel centres that fall inside the patch.
                int left = static_cast<int>(std::floor(minX + 0.5)) - x0;
                int right = static_cast<int>(std::floor(maxX - 0.5)) - x0;
                int top = static_cast<int>(std::floor(minY + 0.5)) - y0;
                int bottom = static_cast<int>(std::floor(maxY - 0.5)) - y0;
                if (right < left) right = left;
                if (bottom < top) bottom = top;
                left = std::clamp(left, 0, boxWidth - 1);
                right = std::clamp(right, 0, boxWidth - 1);
                top = std::clamp(top, 0, boxHeight - 1);
                bottom = std::clamp(bottom, 0, boxHeight - 1);

                bool white = false;
                for (int sy = top; sy <= bottom && !white; ++sy) {
                    const std::uint32_t* source =
                        roomSource_.data() +
                        static_cast<std::size_t>(sy) * boxWidth;
                    for (int sx = left; sx <= right; ++sx) {
                        if ((source[sx] & 0x00FFFFFFu) != 0) {
                            white = true;
                            break;
                        }
                    }
                }
                line[x] = white ? 0xFFFFFFFFu : 0xFF000000u;
            }
        }
    }

    void drawWalker(HDC dc, const RoomLayout& room) {
        const int cell = room.cell;
        const WalkerGeometry geometry = walkerGeometry(room);
        const int scale = geometry.scale;
        const int spriteWidth = geometry.spriteWidth;
        const int spriteHeight = geometry.spriteHeight;
        const int left = room.left + geometry.left;
        const int top = room.top + geometry.top;

        // The smear swells to its full reach mid-step and is gone again by the
        // time the pilgrim lands, so the character never pops in or out of it.
        const double step =
            walker_.moving
                ? std::sin(kPi * std::clamp(walker_.progress, 0.0, 1.0))
                : 0.0;
        const int trailPixels =
            static_cast<int>(std::lround(step * cell * 0.8));
        const int glowRadius = std::max(2, cell / (walker_.moving ? 9 : 11));
        const int pad = std::max(2, cell / 8) * 2 + 2;
        const int trailX = kDirX[walker_.dir] * trailPixels;
        const int trailY = kDirY[walker_.dir] * trailPixels;
        const int originX = std::min(left, left - trailX) - pad;
        const int originY = std::min(top, top - trailY) - pad;
        const int width =
            std::max(left + spriteWidth, left - trailX + spriteWidth) + pad -
            originX;
        const int height =
            std::max(top + spriteHeight, top - trailY + spriteHeight) + pad -
            originY;
        if (!ensureWalkerSurface(width, height)) return;

        const std::size_t count = static_cast<std::size_t>(width) * height;
        std::fill(walkerLight_.begin(), walkerLight_.end(), 0.0f);
        std::fill(walkerTrail_.begin(), walkerTrail_.end(), 0.0f);
        std::fill(walkerGlow_.begin(), walkerGlow_.end(), 0.0f);

        const int spriteLeft = left - originX;
        const int spriteTop = top - originY;
        auto addBlock = [&](std::vector<float>& field, int x, int y,
                            float value) {
            for (int dy = 0; dy < scale; ++dy) {
                const std::size_t offset =
                    static_cast<std::size_t>(y + dy) * width + x;
                for (int dx = 0; dx < scale; ++dx) field[offset + dx] += value;
            }
        };

        std::array<int, kWalkerW * kWalkerH> litX{};
        std::array<int, kWalkerW * kWalkerH> litY{};
        int lit = 0;
        for (int sy = 0; sy < kWalkerH; ++sy) {
            for (int sx = 0; sx < kWalkerW; ++sx) {
                if (!walkerSourcePixel(sx, sy, walker_.dir, walker_.pose)) {
                    continue;
                }
                litX[lit] = spriteLeft + sx * scale;
                litY[lit] = spriteTop + sy * scale;
                addBlock(walkerLight_, litX[lit], litY[lit], 1.0f);
                ++lit;
            }
        }

        // The trail is the same silhouette at the places it has just been.
        // One bucket per sprite cell of travel, so the cost does not grow with
        // the window: the smear only ever needs about a dozen samples. It is
        // deliberately dimmer than the body, so the hood stays readable
        // through its own afterimage.
        if (trailPixels > 0 && lit > 0) {
            const int shiftSpan = std::max(1, trailPixels / scale);
            float weightSum = 0.0f;
            for (int shift = 1; shift <= shiftSpan; ++shift) {
                const float weight = 1.0f - 0.92f * static_cast<float>(shift) /
                                                 static_cast<float>(shiftSpan);
                weightSum += weight;
                const int shiftX = -kDirX[walker_.dir] * shift * scale;
                const int shiftY = -kDirY[walker_.dir] * shift * scale;
                for (int i = 0; i < lit; ++i) {
                    addBlock(walkerTrail_, litX[i] + shiftX,
                             litY[i] + shiftY, weight);
                }
            }
            const float normalize = 0.55f / weightSum;
            for (std::size_t i = 0; i < count; ++i) {
                walkerTrail_[i] *= normalize;
            }
        }

        // The afterimage must not fill the holes in the body (the eyes), or
        // the pilgrim turns into a blob. Flood the transparent border of the
        // sprite; whatever it cannot reach is an interior hole.
        std::array<bool, kWalkerW * kWalkerH> solid{};
        for (int i = 0; i < lit; ++i) {
            solid[(litY[i] - spriteTop) / scale * kWalkerW +
                  (litX[i] - spriteLeft) / scale] = true;
        }
        std::array<bool, kWalkerW * kWalkerH> outside{};
        std::array<int, kWalkerW * kWalkerH> pending{};
        int pendingCount = 0;
        auto open = [&](int x, int y) {
            if (x < 0 || x >= kWalkerW || y < 0 || y >= kWalkerH) return;
            const int index = y * kWalkerW + x;
            if (outside[index] || solid[index]) return;
            outside[index] = true;
            pending[pendingCount++] = index;
        };
        for (int x = 0; x < kWalkerW; ++x) {
            open(x, 0);
            open(x, kWalkerH - 1);
        }
        for (int y = 0; y < kWalkerH; ++y) {
            open(0, y);
            open(kWalkerW - 1, y);
        }
        while (pendingCount > 0) {
            const int index = pending[--pendingCount];
            const int x = index % kWalkerW;
            const int y = index / kWalkerW;
            open(x - 1, y);
            open(x + 1, y);
            open(x, y - 1);
            open(x, y + 1);
        }

        for (std::size_t i = 0; i < count; ++i) {
            walkerGlow_[i] = std::max(walkerLight_[i], walkerTrail_[i]);
        }
        boxBlur(walkerGlow_, walkerScratch_, width, height, glowRadius);
        boxBlur(walkerGlow_, walkerScratch_, width, height, glowRadius);
        // Keep the halo outside the body: the silhouette itself is already
        // solid, and adding the blurred copy on top would only wash it out.
        for (std::size_t i = 0; i < count; ++i) {
            const float body = std::max(walkerLight_[i], walkerTrail_[i]);
            walkerGlow_[i] = std::max(0.0f, walkerGlow_[i] - body);
        }
        // Holes are enclosed, so the blur would otherwise light them up.
        for (int sy = 0; sy < kWalkerH; ++sy) {
            for (int sx = 0; sx < kWalkerW; ++sx) {
                const int index = sy * kWalkerW + sx;
                if (outside[index] || solid[index]) continue;
                const int x = spriteLeft + sx * scale;
                const int y = spriteTop + sy * scale;
                for (int dy = 0; dy < scale; ++dy) {
                    const std::size_t offset =
                        static_cast<std::size_t>(y + dy) * width + x;
                    for (int dx = 0; dx < scale; ++dx) {
                        walkerTrail_[offset + dx] = 0.0f;
                        walkerGlow_[offset + dx] = 0.0f;
                    }
                }
            }
        }

        static constexpr int kBayer[8][8] = {
            { 0, 32,  8, 40,  2, 34, 10, 42},
            {48, 16, 56, 24, 50, 18, 58, 26},
            {12, 44,  4, 36, 14, 46,  6, 38},
            {60, 28, 52, 20, 62, 30, 54, 22},
            { 3, 35, 11, 43,  1, 33,  9, 41},
            {51, 19, 59, 27, 49, 17, 57, 25},
            {15, 47,  7, 39, 13, 45,  5, 37},
            {63, 31, 55, 23, 61, 29, 53, 21},
        };
        const float glowGain =
            (walker_.moving ? 1.6f : 0.9f) *
            (0.94f + 0.06f * static_cast<float>(std::sin(nowMs() * 0.0016)));
        for (int y = 0; y < height; ++y) {
            const int* thresholds = kBayer[(originY + y) & 7];
            std::uint32_t* out =
                walkerPixels_ + static_cast<std::size_t>(y) * width;
            const std::size_t base = static_cast<std::size_t>(y) * width;
            for (int x = 0; x < width; ++x) {
                const std::size_t index = base + x;
                const float light = walkerLight_[index] +
                                    walkerTrail_[index] +
                                    walkerGlow_[index] * glowGain;
                // The dither grid is pinned to the screen, not the sprite, so
                // the halo does not crawl as the pilgrim walks.
                out[x] = light * 64.0f >
                                 static_cast<float>(thresholds[(originX + x) & 7])
                             ? 0xFFFFFFFFu
                             : 0xFF000000u;
            }
        }

        // Only the white dots land on the room; black stays transparent.
        BitBlt(dc, originX, originY, width, height, walkerDc_, 0, 0, SRCPAINT);
    }

    void paintLibrary(HDC dc, int width, int height) {
        const int scale = static_cast<int>(dpi_);
        const int logoCell = std::max(2, MulDiv(5, scale, 96));
        int ruleY = 0;
        int titleY = 0;
        drawHeader(dc, width, L"BETHELAMENT", titleFont_, logoCell, ruleY,
                   titleY);

        // The room's name sits at the right end of the header rule.
        const int margin = MulDiv(34, scale, 96);
        const int smallH = textHeight(dc, smallFont_);
        drawTextAt(dc, smallFont_, kRooms[roomId_].title, width - margin,
                   ruleY - smallH - MulDiv(4, scale, 96), kWhite,
                   TA_RIGHT | TA_TOP);

        const RoomLayout room = roomLayout(width, height);
        const int roomWidth = room.cell * kRoomCols;
        const int roomHeight = room.cell * kRoomRows;
        const WalkerGeometry geometry = walkerGeometry(room);
        const double step =
            walker_.moving
                ? std::sin(kPi * std::clamp(walker_.progress, 0.0, 1.0))
                : 0.0;
        // The lens sits over the body, not the feet, and strengthens mid-step.
        const int lensX = geometry.feetX;
        const int lensY = geometry.feetY - geometry.spriteHeight / 2;
        const int lensRadius =
            std::max(8, static_cast<int>(std::lround(room.cell * 1.9)));
        const double lensStrength = 0.75 + 0.25 * step;

        // The room sits on black; the rings keep to the wall around it. It is
        // drawn offscreen first so the lens can bend the floor around the
        // pilgrim, then copied back in one go.
        if (ensureRoomSurface(roomWidth, roomHeight)) {
            drawRoom(roomDc_, RoomLayout{0, 0, room.cell});
            bendRoom(lensX, lensY, lensRadius, lensStrength);
            BitBlt(dc, room.left, room.top, roomWidth, roomHeight, roomDc_, 0,
                   0, SRCCOPY);
        } else {
            fillColor(dc, RECT{room.left, room.top, room.left + roomWidth,
                               room.top + roomHeight},
                      kBlack);
            drawRoom(dc, room);
        }
        drawWalker(dc, room);

        // One line under the room says what the room wants, or what just
        // answered. A notice from the last step wins for a few seconds.
        std::wstring line = kRooms[roomId_].hint;
        if (roomId_ == 0 && key_) {
            line = L"The key is warm. The north door stands open.";
        } else if (roomId_ == 1 && leverSolved_) {
            line = L"The levers hold. The north door stands open.";
        } else if (roomId_ == 2 && plateSolved_) {
            line = L"The plates are quiet. The north door stands open.";
        } else if (roomId_ == 3) {
            line = L"The tear is open. Walk into it.";
        }
        if (!notice_.empty() && nowMs() - noticeStart_ < 4200) {
            line = notice_;
        }
        const int hudY = room.top + roomHeight + MulDiv(8, scale, 96);
        if (hudY + smallH <= height) {
            drawTextCentered(dc, smallFont_, line, width / 2, hudY, kWhite);
        }
        // Controls and the state of the room's lock, at the two lower corners.
        const int controlsY = height - smallH - MulDiv(10, scale, 96);
        drawTextAt(dc, smallFont_, L"ARROWS MOVE   E INTERACT   R RESTART",
                   margin, controlsY, kWhite);
        std::wstring state;
        if (roomId_ == 0) {
            state = key_ ? L"KEY TAKEN" : L"KEY: MISSING";
        } else if (roomId_ == 1) {
            state = L"LEVERS ";
            for (int i = 0; i < kLeverCount; ++i) {
                state += levers_[i] ? L"^ " : L"v ";
            }
            state += leverSolved_ ? L"OPEN" : L"SHUT";
        } else if (roomId_ == 2) {
            state = L"PLATES ";
            for (int i = 0; i < 3; ++i) {
                state += plateSolved_ || i < plateStep_ ? L"#" : L".";
            }
            state += plateSolved_ ? L" OPEN" : L" SHUT";
        } else {
            state = L"SEARCHING";
        }
        drawTextAt(dc, smallFont_, state, width - margin, controlsY, kWhite,
                   TA_RIGHT | TA_TOP);
    }

    // ------------------------------------------------------------- the break
    // After the breach the game stops being a game. First the frame tears and
    // the crash dialog arrives, then a search page repeats one answer forever,
    // then the answer turns out to be a corridor with no floor plan.

    static int glitchHash(int x, int y, int seed) {
        std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u +
                          static_cast<std::uint32_t>(y) * 668265263u +
                          static_cast<std::uint32_t>(seed) * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return static_cast<int>((h ^ (h >> 16)) & 0x7FFFFFFF);
    }

    // The same text, with letters stuttering over themselves.
    static std::wstring repeatGlitch(const std::wstring& text, int seed,
                                     int amount) {
        std::wstring out;
        out.reserve(text.size() + text.size() / 6);
        int index = 0;
        for (const wchar_t ch : text) {
            out.push_back(ch);
            if (ch != L' ' && ch != L'-' &&
                glitchHash(index, seed, amount) % 9 == 0) {
                out.push_back(ch);
            }
            ++index;
        }
        return out;
    }

    void startBreak() {
        if (phase_ != GamePhase::Play) return;
        phase_ = GamePhase::Crash;
        phaseStart_ = nowMs();
        lastDir_ = -1;
        walker_.moving = false;
        if (hwnd_) SetWindowTextW(hwnd_, L"Bethelament.exe (Not Responding)");
        invalidate();
    }

    void beginSearch() {
        phase_ = GamePhase::Search;
        phaseStart_ = nowMs();
        if (hwnd_) SetWindowTextW(hwnd_, L"Veil Search (Not Responding)");
        invalidate();
    }

    void beginCorridor() {
        phase_ = GamePhase::Corridor;
        phaseStart_ = nowMs();
        camX_ = 15.5;
        camY_ = 12.5;
        camAngle_ = 0.0;
        schoolHearts_.fill(0);
        schoolRounds_.fill(0);
        schoolExpressions_.fill(SchoolExpression::Neutral);
        schoolReactionUntil_.fill(0);
        schoolHorror_.reset();
        schoolTalking_ = schoolDate_ = -1;
        schoolRizz_ = 0;
        schoolNotice_ = L"Meet the college club members. Three hearts unlock a festival invitation.";
        schoolNoticeStart_ = nowMs();
        gameTickLast_ = nowMs();
        corridorTickLast_ = nowMs();
        if (hwnd_) SetWindowTextW(hwnd_, L"Anaise - After School");
        audio_.play(anaise::Music::School);
        invalidate();
    }

    void endBreak() {
        phase_ = GamePhase::Play;
        resetGame();
        audio_.play(anaise::Music::Room);
    }

    void paintBreak(HDC dc, int width, int height) {
        switch (phase_) {
            case GamePhase::Crash: paintCrash(dc, width, height); break;
            case GamePhase::Search: paintSearch(dc, width, height); break;
            case GamePhase::Corridor: paintCorridor(dc, width, height); break;
            default: break;
        }
    }

    // The crash: the library frame survives in horizontal bands, some of them
    // shifted, some inverted, with a second copy ghosting over the top.
    void paintCrash(HDC dc, int width, int height) {
        const double t = static_cast<double>(nowMs() - phaseStart_);
        const double intensity = std::clamp((t - 280.0) / 1500.0, 0.0, 1.0);
        if (!ensureBreakSurface(width, height)) return;
        fillColor(breakDc_, RECT{0, 0, width, height}, kBlack);
        paintLibrary(breakDc_, width, height);
        GdiFlush();

        if (intensity <= 0.0) {
            BitBlt(dc, 0, 0, width, height, breakDc_, 0, 0, SRCCOPY);
        } else {
            const int maxBand = 4 + static_cast<int>(intensity * 24.0);
            for (int y = 0; y < height;) {
                const int band =
                    2 + glitchHash(0, y, 7) % std::max(3, maxBand);
                int shift = 0;
                if (glitchHash(y, 3, 11) % 100 < 55 + intensity * 30.0) {
                    shift = (glitchHash(y, 5, 3) % 41) - 20;
                    shift = static_cast<int>(shift * intensity * 1.6);
                }
                const DWORD rop =
                    glitchHash(y, 9, 13) % 100 < 8 + intensity * 10.0
                        ? NOTSRCCOPY
                        : SRCCOPY;
                BitBlt(dc, shift, y, width, band, breakDc_, 0, y, rop);
                y += band;
            }
            if (intensity > 0.35) {
                const int offset = static_cast<int>(6 + intensity * 30.0);
                const int sway = static_cast<int>(
                    std::sin(t * 0.004) * 12.0 * intensity);
                BitBlt(dc, offset + sway, 0, width - offset, height,
                       breakDc_, 0, 0, SRCPAINT);
            }
        }

        // Scanlines crawl over the wreck, and a few of them are white.
        HBRUSH black = CreateSolidBrush(kBlack);
        const int spacing = std::max(2, px(3));
        for (int y = 0; y < height; y += spacing) {
            if (glitchHash(y, 1, 5) % 3 == 0) continue;
            RECT line{0, y, width, y + 1};
            FillRect(dc, &line, black);
        }
        DeleteObject(black);
        if (intensity > 0.2 && glitchHash(int(t / 90), 2, 17) % 4 == 0) {
            const int y = glitchHash(int(t / 90), 4, 19) % height;
            fillColor(dc, RECT{0, y, width, y + std::max(2, px(2))}, kWhite);
        }

        // Ghosts of the pilgrim, drifting where the game used to be.
        const int ghostScale = std::max(2, px(3));
        const int drift =
            static_cast<int>(std::sin(t * 0.0016) * width * 0.05);
        drawGhostFigure(dc, width / 3 + drift, height * 3 / 4, ghostScale,
                        kFaceDown, kStandPose, 1, 3);
        drawGhostFigure(dc, width * 2 / 3 - drift, height * 4 / 5, ghostScale,
                        kFaceLeft, kStandPose, 2, 4);
        drawGhostFigure(dc, width / 2 - drift / 2, height * 2 / 3, ghostScale,
                        kFaceDown, kStandPose, 3, 6);

        const int smallH = textHeight(dc, smallFont_);
        drawTextAt(dc, smallFont_, L"FAULT 0xC0000005  //  PAGE 0x00000000",
                   px(16), height - smallH - px(12), kWhite);

        if (t > 1200.0) drawCrashDialog(dc, width, height, t);
    }

    void drawCrashDialog(HDC dc, int width, int height, double t) {
        const int w = std::min(px(560), std::max(px(300), width - px(80)));
        const int h = std::min(px(210), std::max(px(150), height - px(80)));
        const int left = (width - w) / 2 +
                         static_cast<int>(std::sin(t * 0.003) * px(3));
        const int top = (height - h) / 2;
        const RECT box{left, top, left + w, top + h};
        fillDither(dc, RECT{box.left + px(6), box.top + px(6),
                            box.right + px(6), box.bottom + px(6)}, 5);
        fillColor(dc, box, kBlack);
        strokeRect(dc, box, kWhite, std::max(1, px(2)));

        const int barH = px(26);
        const RECT bar{box.left + px(2), box.top + px(2), box.right - px(2),
                       box.top + barH};
        fillColor(dc, bar, kWhite);
        drawTextAt(dc, smallFont_,
                   L"BETHELAMENT.EXE  -  NOT RESPONDING",
                   bar.left + px(8), bar.top + (barH - textHeight(dc, smallFont_)) / 2,
                   kBlack);
        const int closeSize = px(14);
        RECT close{bar.right - closeSize - px(6),
                   bar.top + (barH - closeSize) / 2,
                   bar.right - px(6),
                   bar.top + (barH + closeSize) / 2};
        strokeRect(dc, close, kBlack, 1);
        drawLine(dc, close.left + px(4), close.top + px(4),
                 close.right - px(4), close.bottom - px(4), kBlack);
        drawLine(dc, close.right - px(4), close.top + px(4),
                 close.left + px(4), close.bottom - px(4), kBlack);

        const int textLeft = box.left + px(18);
        int y = bar.bottom + px(14);
        const int bodyH = textHeight(dc, bodyFont_);
        drawTextAt(dc, bodyFont_, L"The instruction at 0x004F13A0", textLeft, y,
                   kWhite);
        y += bodyH;
        drawTextAt(dc, bodyFont_, L"referenced memory at 0x00000000.", textLeft,
                   y, kWhite);
        y += bodyH;
        drawTextAt(dc, bodyFont_, L"The memory could not be \"read\".", textLeft,
                   y, kWhite);
        y += bodyH + px(6);
        drawTextAt(dc, smallFont_, L"ROOM III WAS OPEN. THE ROOM IS STILL OPEN.",
                   textLeft, y, kWhite);

        const bool live = t > 2600.0;
        const int buttonH = px(24);
        const int buttonY = box.bottom - buttonH - px(14);
        RECT search{box.right - px(154), buttonY, box.right - px(16),
                    buttonY + buttonH};
        RECT report{search.left - px(132), buttonY, search.left - px(12),
                    buttonY + buttonH};
        strokeRect(dc, report, kWhite, 1);
        drawTextCentered(dc, smallFont_, L"SEND REPORT",
                         (report.left + report.right) / 2,
                         report.top + (buttonH - textHeight(dc, smallFont_)) / 2,
                         kWhite);
        if (live) {
            fillColor(dc, search, kWhite);
            drawTextCentered(dc, smallFont_, L"SEARCH ONLINE",
                             (search.left + search.right) / 2,
                             search.top +
                                 (buttonH - textHeight(dc, smallFont_)) / 2,
                             kBlack);
        } else {
            strokeRect(dc, search, kWhite, 1);
            drawTextCentered(dc, smallFont_, L"SEARCH ONLINE",
                             (search.left + search.right) / 2,
                             search.top +
                                 (buttonH - textHeight(dc, smallFont_)) / 2,
                             kWhite);
        }
        // A cursor that arrives late and blinks over the live button.
        if (live) {
            const int blink = static_cast<int>(t / 420) % 2;
            if (blink == 0) {
                const int cx = search.left + px(12);
                const int cy = search.top + buttonH / 2;
                drawLine(dc, cx, cy, cx + px(8), cy + px(8), kBlack);
                drawLine(dc, cx + px(8), cy + px(8), cx + px(3), cy + px(8),
                         kBlack);
            }
            drawTextAt(dc, smallFont_, L"SPACE", box.left + px(18),
                       box.bottom - textHeight(dc, smallFont_) - px(16), kWhite);
        }
    }

    // The search: one query, one answer, repeated until the page stops being a
    // page. The veil keeps fetching the same room.
    void paintSearch(HDC dc, int width, int height) {
        const double t = static_cast<double>(nowMs() - phaseStart_);
        const int margin = px(18);
        const int shake =
            static_cast<int>(std::sin(t * 0.010) * (t > 3200.0 ? px(4) : 0));
        const int left = margin + shake;
        const int top = margin;
        const int right = width - margin - shake;
        const int bottom = height - margin;
        const RECT frame{left, top, right, bottom};
        fillColor(dc, frame, kBlack);
        strokeRect(dc, frame, kWhite, 1);

        const int barH = px(26);
        const int smallH = textHeight(dc, smallFont_);
        const int bodyH = textHeight(dc, bodyFont_);
        fillColor(dc, RECT{left + 1, top + 1, right - 1, top + barH}, kWhite);
        drawTextAt(dc, smallFont_, L"VEIL  //  SEARCH  -  NOT RESPONDING",
                   left + px(8), top + (barH - smallH) / 2, kBlack);
        for (int i = 0; i < 3; ++i) {
            RECT control{right - px(8) - (i + 1) * px(18), top + px(7),
                         right - px(8) - (i + 1) * px(18) + px(12),
                         top + px(19)};
            strokeRect(dc, control, kBlack, 1);
        }

        const int addrTop = top + barH + px(8);
        const int addrH = px(24);
        const RECT address{left + px(10), addrTop, right - px(10),
                           addrTop + addrH};
        strokeRect(dc, address, kWhite, 1);
        drawTextAt(dc, smallFont_, L"veil://find?q=why+does+bethelament+repeat",
                   address.left + px(8), address.top + (addrH - smallH) / 2,
                   kWhite);

        const std::wstring query = L"why does bethelament repeat";
        const std::size_t typed = std::min<std::size_t>(
            query.size(),
            static_cast<std::size_t>(std::max(0.0, t - 260.0) / 52.0));
        std::wstring shown = L"> " + query.substr(0, typed);
        if (static_cast<int>(t / 280) % 2 == 0) shown += L"_";
        const int queryY = addrTop + addrH + px(18);
        drawTextAt(dc, bodyFont_, shown, left + px(18), queryY, kWhite);

        const bool late = t > 2500.0;
        const int rowH = px(58);
        int y = queryY + bodyH + px(18);
        for (int i = 0; i < 7 && y + rowH <= bottom - px(52); ++i, y += rowH) {
            std::wstring title =
                L"bethelament - room iii - the pilgrim is still in the room";
            std::wstring snippet =
                L"the room is the same room the room is the same room";
            if (late) {
                title = repeatGlitch(title, i, static_cast<int>(t / 320));
                snippet = repeatGlitch(snippet, i + 7,
                                       static_cast<int>(t / 260));
            }
            drawTextAt(dc, bodyFont_, title, left + px(18), y, kWhite);
            drawTextAt(dc, smallFont_, L"veil://bethelament/room/3/repeat",
                       left + px(18), y + px(20), kWhite);
            drawTextAt(dc, smallFont_, snippet, left + px(18), y + px(36),
                       kWhite);
        }

        // The footer insists the result set is exactly one page long.
        std::wstring pages = L"RESULTS 1-1 OF 1    ";
        for (int i = 0; i < 12; ++i) pages += L"1 ";
        const int statusY = bottom - smallH - px(10);
        drawTextAt(dc, smallFont_, pages, left + px(18), statusY, kWhite);
        drawTextAt(dc, smallFont_, L"STILL SEARCHING...", right - px(18),
                   statusY, kWhite, TA_RIGHT | TA_TOP);

        // Loose bands of noise at the margins, as if the page is shedding.
        for (int i = 0; i < 6; ++i) {
            const int nx = left + px(4) + glitchHash(i, 1, 21) % (right - left - px(8));
            const int ny = top + px(4) + glitchHash(i, 2, 23) % (bottom - top - px(8));
            fillDither(dc, RECT{nx, ny, nx + px(30) + (i * 13) % px(60),
                                ny + px(2)}, 3);
        }

        if (t > 5200.0) {
            fillColor(dc, RECT{left + 1, top + barH + 1, right - 1, bottom - 1},
                      kBlack);
            if (static_cast<int>(t / 140) % 2 == 0) {
                drawTextCentered(dc, headingFont_, L"FOUND: AFTER SCHOOL",
                                 (left + right) / 2, (top + bottom) / 2 - px(20),
                                 kWhite);
            }
            drawTextCentered(dc, smallFont_, L"GEOMETRY RESTORED IN 0.9 SECONDS",
                             (left + right) / 2, (top + bottom) / 2 + px(24),
                             kWhite);
        }
    }

    static bool corridorSolid(int x, int y) {
        if (x <= 0 || x >= kCorridorCols - 1 || y <= 0 || y >= kCorridorRows - 1) return true;
        // Broad halls connect the college classrooms and four clubs.
        if (y == 9 || y == 15) return !(x == 5 || x == 15 || x == 25);
        if ((x == 10 || x == 20) && (y < 9 || y > 15)) return true;
        if (y == 4 || y == 20) return !(x == 5 || x == 15 || x == 25);
        // Desks leave generous paths from every doorway to each club member.
        if ((y == 2 || y == 22) && x % 10 >= 2 && x % 10 <= 3) return true;
        return false;
    }

    void updateCorridor(double elapsed) {
        if (hwnd_ && GetForegroundWindow() != hwnd_) return;
        if (!hwnd_) return;
        auto held = [](int key) { return (GetKeyState(key) & 0x8000) != 0; };
        const double turn = (held(VK_RIGHT) || held('D') ? 1.0 : 0.0) -
                            (held(VK_LEFT) || held('A') ? 1.0 : 0.0);
        const double move = (held(VK_UP) || held('W') ? 1.0 : 0.0) -
                            (held(VK_DOWN) || held('S') ? 1.0 : 0.0);
        moveCorridor(elapsed, move * (held(VK_SHIFT) ? 1.65 : 1.0), turn);
        if (nowMs() - phaseStart_ >= kCorridorBootMs && schoolDate_ < 0) {
            schoolHorror_.tick(elapsed, nowMs(), camX_, camY_, corridorSolid);
            flushSchoolSounds();
        }
    }

    void moveCorridor(double elapsed, double move, double turn) {
        if (schoolDate_ >= 0 || schoolTalking_ >= 0 || nowMs() - phaseStart_ < kCorridorBootMs) return;
        const double dt = std::clamp(elapsed / 1000.0, 0.0, 0.12);
        camAngle_ = std::remainder(camAngle_ + turn * 2.4 * dt, 2.0 * kPi);
        if (move == 0.0) return;
        const double speed = 2.1 * move * dt;
        const double nx = camX_ + std::cos(camAngle_) * speed;
        const double ny = camY_ + std::sin(camAngle_) * speed;
        auto freeAt = [this](double x, double y) {
            constexpr double kRadius = 0.22;
            for (const auto& actor : schoolHorror_.actors)
                if (!actor.escaped && actor.health > 0 && std::hypot(x - actor.x, y - actor.y) < 0.42) return false;
            return !corridorSolid(static_cast<int>(std::floor(x - kRadius)),
                                  static_cast<int>(std::floor(y - kRadius))) &&
                   !corridorSolid(static_cast<int>(std::floor(x + kRadius)),
                                  static_cast<int>(std::floor(y - kRadius))) &&
                   !corridorSolid(static_cast<int>(std::floor(x - kRadius)),
                                  static_cast<int>(std::floor(y + kRadius))) &&
                   !corridorSolid(static_cast<int>(std::floor(x + kRadius)),
                                  static_cast<int>(std::floor(y + kRadius)));
        };
        if (freeAt(nx, camY_)) camX_ = nx;
        if (freeAt(camX_, ny)) camY_ = ny;
    }

    int schoolTarget() const {
        int target = -1;
        double closest = 1.8;
        for (int i = 0; i < 4; ++i) {
            const auto& girl = schoolHorror_.actors[i];
            if (girl.health == 0 || girl.fleeing || girl.escaped) continue;
            const double distance = std::hypot(camX_ - girl.x, camY_ - girl.y);
            if (distance >= closest) continue;
            bool visible = true;
            for (int step = 1; step <= 16; ++step) {
                const double f = step / 16.0;
                if (corridorSolid(static_cast<int>(camX_ + (girl.x - camX_) * f),
                                  static_cast<int>(camY_ + (girl.y - camY_) * f))) visible = false;
            }
            if (visible) { target = i; closest = distance; }
        }
        return target;
    }

    void flushSchoolSounds() {
        const unsigned sounds = schoolHorror_.takeSounds();
        audio_.effects(sounds);
        if (sounds & 64u) audio_.schoolScream(false);
        if (sounds & 128u) audio_.schoolScream(true);
        audio_.play(schoolHorror_.alarm ? anaise::Music::SchoolHorror : anaise::Music::School);
        if (schoolTalking_ >= 0 && schoolHorror_.actors[schoolTalking_].fleeing) schoolTalking_ = -1;
    }

    void attackSchool() {
        if (phase_ != GamePhase::Corridor || schoolDate_ >= 0 || nowMs() - phaseStart_ < kCorridorBootMs) return;
        const int victim = schoolHorror_.attack(nowMs(), camX_, camY_, camAngle_, corridorSolid);
        if (schoolHorror_.equipped) schoolTalking_ = -1;
        if (victim >= 0) {
            schoolHearts_.fill(0); schoolRizz_ = 0;
            schoolNotice_ = schoolHorror_.actors[victim].health > 0 ? L"A scream tears through the hall. Footsteps scatter."
                : L"The music keeps playing. Nobody answers.";
            schoolNoticeStart_ = nowMs();
        }
        flushSchoolSounds(); invalidate();
    }

    void paintSchoolViolence(HDC dc, int width, int height) const {
        // The palette stays binary: bright liquid and black cutouts instead of red.
        for (const auto& burst : schoolHorror_.blood) {
            const double age = static_cast<double>(nowMs() - burst.when) / 1000.0;
            if (age < 0 || age > 1.8) continue;
            const double fade = 1.0 - age / 1.8;
            for (int i = 0; i < 100; ++i) {
                const int hash = glitchHash(i, static_cast<int>(burst.seed), 27);
                const double angle = (hash % 6283) / 1000.0;
                const double speed = px(80 + (hash / 17) % 400);
                const int x = width / 2 + static_cast<int>(std::cos(angle) * speed * age);
                const int y = height / 2 + static_cast<int>(std::sin(angle) * speed * age + px(160) * age * age);
                const int size = std::max(1, static_cast<int>(px(2 + hash % 7) * fade));
                fillColor(dc, RECT{x, y, x + size, y + size * 2}, kWhite);
            }
        }
        if (!schoolHorror_.equipped) return;
        double swing = 0;
        if (schoolHorror_.swings && nowMs() - schoolHorror_.swingAt < 300)
            swing = std::sin((nowMs() - schoolHorror_.swingAt) / 300.0 * kPi);
        const int unit = px(1);
        const int cx = width - px(120) - static_cast<int>(px(130) * swing);
        const int cy = height - px(72) - static_cast<int>(px(90) * swing);
        POINT blade[] = {{cx - 14 * unit, cy - 38 * unit}, {cx - 40 * unit, cy - 180 * unit},
                         {cx + 10 * unit, cy - 80 * unit}, {cx + 16 * unit, cy - 40 * unit}};
        HGDIOBJ oldPen = SelectObject(dc, GetStockObject(WHITE_PEN));
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(WHITE_BRUSH));
        Polygon(dc, blade, 4);
        drawLine(dc, cx - 32 * unit, cy - 150 * unit, cx + 6 * unit, cy - 67 * unit, kBlack);
        fillColor(dc, RECT{cx - 21 * unit, cy - 42 * unit, cx + 25 * unit, cy - 31 * unit}, kWhite);
        fillColor(dc, RECT{cx - 12 * unit, cy - 30 * unit, cx + 17 * unit, cy + 40 * unit}, kBlack);
        strokeRect(dc, RECT{cx - 12 * unit, cy - 30 * unit, cx + 17 * unit, cy + 40 * unit}, kWhite, 2);
        for (int i = 0; i < 5; ++i) drawLine(dc, cx - 8 * unit, cy - 21 * unit + i * 11 * unit,
            cx + 13 * unit, cy - 21 * unit + i * 11 * unit);
        if (!schoolHorror_.blood.empty()) {
            for (int i = 0; i < 12; ++i) {
                const int x = cx - (5 + i % 4) * unit, y = cy - (60 + i * 7) * unit;
                fillColor(dc, RECT{x, y, x + 3 * unit, y + 8 * unit}, kBlack);
            }
        }
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush);
    }

    void interactSchool() {
        if (schoolDate_ >= 0 || schoolTalking_ >= 0 || nowMs() - phaseStart_ < kCorridorBootMs) return;
        if (schoolHorror_.pickup(camX_, camY_)) {
            schoolNotice_ = L"A knife under the desk. Q equip / put away. F or left click use.";
            schoolNoticeStart_ = nowMs(); invalidate(); return;
        }
        schoolTalking_ = schoolTarget();
        if (schoolTalking_ < 0) {
            schoolNotice_ = L"Walk up to a club member, then press E to talk.";
            schoolNoticeStart_ = nowMs();
        }
        invalidate();
    }

    void chooseSchoolReply(int choice) {
        if (choice < 0 || choice > 2 || schoolTalking_ < 0 || schoolDate_ >= 0) return;
        const int girl = schoolTalking_;
        if (schoolRounds_[girl] >= 3) {
            if (choice == 0 && schoolHearts_[girl] >= 3) {
                schoolDate_ = girl;
                schoolExpressions_[girl] = SchoolExpression::Shy;
            }
            else if (choice == 1) schoolRounds_[girl] = 0;
            else if (choice == 0) {
                schoolExpressions_[girl] = SchoolExpression::Hurt;
                schoolReactionUntil_[girl] = nowMs() + 6500;
                schoolNotice_ = L"Let's get to know each other first. Talk a little more?";
                schoolNoticeStart_ = nowMs();
            }
        } else {
            const auto& talk = kSchoolTalks[girl][schoolRounds_[girl]];
            const bool best = choice == talk.best;
            const int rudeChoice = (girl == 0 && schoolRounds_[girl] == 1) ||
                (girl == 1 && schoolRounds_[girl] == 2) || (girl == 3 && schoolRounds_[girl] == 1)
                ? 0 : (talk.best + 1) % 3;
            const bool rude = choice == rudeChoice;
            schoolExpressions_[girl] = best ? SchoolExpression::Happy : rude ? SchoolExpression::Hurt : SchoolExpression::Shy;
            schoolReactionUntil_[girl] = nowMs() + 6500;
            schoolHearts_[girl] = std::clamp(schoolHearts_[girl] + (best ? 2 : rude ? -1 : 1), 0, 5);
            schoolRizz_ = std::clamp(schoolRizz_ + (best ? 10 : rude ? -5 : 3), 0, 100);
            ++schoolRounds_[girl];
            schoolNotice_ = std::wstring(kSchoolGirls[girl].name) + L": " +
                (best ? kSchoolGirls[girl].reply : rude ? L"That was a little harsh. Let's start over." : L"Thanks. It's nice getting to know you.");
            schoolNoticeStart_ = nowMs();
        }
        schoolTalking_ = -1;
        invalidate();
    }

    bool ensureSchoolFaces() {
        bool ready = true;
        for (int girl = 0; girl < 4; ++girl) {
            auto& pixels = schoolFacePixels_[girl];
            if (!pixels.empty()) continue;
            HBITMAP bitmap = LoadBitmapW(instance_, MAKEINTRESOURCEW(401 + girl));
            if (!bitmap) { ready = false; continue; }
            BITMAP details{};
            GetObjectW(bitmap, sizeof(details), &details);
            const int width = kSchoolFaceSize * kSchoolFaceExpressions;
            if (details.bmWidth != width || details.bmHeight != kSchoolFaceSize) {
                DeleteObject(bitmap); ready = false; continue;
            }
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -kSchoolFaceSize;
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            pixels.resize(static_cast<std::size_t>(width) * kSchoolFaceSize);
            HDC dc = CreateCompatibleDC(nullptr);
            const int lines = dc ? GetDIBits(dc, bitmap, 0, kSchoolFaceSize, pixels.data(), &info, DIB_RGB_COLORS) : 0;
            if (dc) DeleteDC(dc);
            DeleteObject(bitmap);
            if (lines != kSchoolFaceSize) { pixels.clear(); ready = false; }
        }
        return ready;
    }

    SchoolExpression schoolExpression(int girl) const {
        if (schoolHorror_.actors[girl].health == 0) return SchoolExpression::Dead;
        if (schoolHorror_.actors[girl].fleeing) return SchoolExpression::Afraid;
        if (schoolDate_ == girl) return SchoolExpression::Shy;
        if (nowMs() < schoolReactionUntil_[girl]) return schoolExpressions_[girl];
        return schoolHearts_[girl] >= 3 ? SchoolExpression::Happy : SchoolExpression::Neutral;
    }

    void drawSchoolPortrait(HDC dc, int girl, const RECT& bounds) const {
        const auto& pixels = schoolFacePixels_[girl];
        if (pixels.empty()) return;
        const int expression = static_cast<int>(schoolExpression(girl));
        const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
        if (width <= 0 || height <= 0) return;
        std::vector<std::uint32_t> rendered(static_cast<std::size_t>(width) * height);
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            const int u = x * kSchoolFaceSize / width, v = y * kSchoolFaceSize / height;
            const int luminance = pixels[static_cast<std::size_t>(v) * kSchoolFaceSize * kSchoolFaceExpressions + expression * kSchoolFaceSize + u] & 255;
            const int threshold = kBayer8[(bounds.top + y) & 7][(bounds.left + x) & 7] * 4 + 2;
            rendered[static_cast<std::size_t>(y) * width + x] = luminance > threshold ? 0xFFFFFFFFu : 0xFF000000u;
        }
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        const int saved = SaveDC(dc);
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
            0, 0, width, height, rendered.data(), &info, DIB_RGB_COLORS, SRCCOPY);
        RestoreDC(dc, saved);
        strokeRect(dc, bounds, kWhite, 1);
    }

    void drawSchoolHearts(HDC dc, int x, int y, int hearts) const {
        static const char* rows[] = {".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."};
        const int unit = px(2);
        auto inside = [&](int col, int row) { return row >= 0 && row < 6 && col >= 0 && col < 7 && rows[row][col] == '#'; };
        for (int heart = 0; heart < 5; ++heart)
            for (int row = 0; row < 6; ++row) for (int col = 0; col < 7; ++col)
                if (inside(col, row) && (heart < hearts || !inside(col - 1, row) || !inside(col + 1, row) ||
                    !inside(col, row - 1) || !inside(col, row + 1)))
                    fillColor(dc, RECT{x + heart * unit * 9 + col * unit, y + row * unit,
                        x + heart * unit * 9 + (col + 1) * unit, y + (row + 1) * unit}, kWhite);
    }

    void paintSchoolMap(HDC dc) const {
        const int cell = px(3), left = px(16), top = px(46);
        fillColor(dc, RECT{left - px(4), top - px(4), left + kCorridorCols * cell + px(4),
            top + kCorridorRows * cell + px(4)}, kBlack);
        for (int y = 0; y < kCorridorRows; ++y) for (int x = 0; x < kCorridorCols; ++x)
            if (corridorSolid(x, y)) fillColor(dc, RECT{left + x * cell, top + y * cell,
                left + (x + 1) * cell, top + (y + 1) * cell}, kWhite);
        for (const auto& girl : schoolHorror_.actors) {
            if (girl.escaped) continue;
            const int x = left + static_cast<int>(girl.x * cell), y = top + static_cast<int>(girl.y * cell);
            fillColor(dc, RECT{x - 1, y - 1, x + 2, y + 2}, kWhite);
        }
        const int cx = left + static_cast<int>(camX_ * cell), cy = top + static_cast<int>(camY_ * cell);
        strokeRect(dc, RECT{cx - 2, cy - 2, cx + 3, cy + 3}, kWhite, 1);
        drawLine(dc, cx, cy, cx + static_cast<int>(std::cos(camAngle_) * cell * 3),
                 cy + static_cast<int>(std::sin(camAngle_) * cell * 3));
        drawTextAt(dc, smallFont_, L"CAMPUS MAP", left, top + kCorridorRows * cell + px(6), kWhite);
    }

    void paintCorridor(HDC dc, int width, int height) {
        // The rest of the shell owns its font settings. This scene uses crisp
        // bitmap text so the school HUD also stays strictly black and white.
        struct CrispFont {
            HFONT& slot;
            HFONT original;
            explicit CrispFont(HFONT& font) : slot(font), original(font) {
                LOGFONTW lf{};
                if (GetObjectW(font, sizeof(lf), &lf)) {
                    lf.lfQuality = NONANTIALIASED_QUALITY;
                    if (HFONT crisp = CreateFontIndirectW(&lf)) slot = crisp;
                }
            }
            ~CrispFont() { if (slot != original) DeleteObject(slot); slot = original; }
        } small(smallFont_), body(bodyFont_), heading(headingFont_);
        const double t = static_cast<double>(nowMs() - phaseStart_);
        if (t < static_cast<double>(kCorridorBootMs)) {
            fillColor(dc, RECT{0, 0, width, height}, kBlack);
            drawTextCentered(dc, headingFont_, L"AFTER SCHOOL", width / 2, height / 2 - px(22), kWhite);
            drawTextCentered(dc, smallFont_, L"A NEW TERM. A SECOND CHANCE.", width / 2, height / 2 + px(22), kWhite);
            return;
        }
        renderCorridor(dc, width, height);
        const int smallH = textHeight(dc, smallFont_);
        fillColor(dc, RECT{0, 0, width, px(38)}, kBlack);
        drawTextAt(dc, smallFont_, L"ANAISE COLLEGE / FESTIVAL FRIDAY", px(16), px(12), kWhite);
        drawTextAt(dc, smallFont_, L"RIZZ " + std::to_wstring(schoolRizz_), width - px(16), px(12), kWhite, TA_RIGHT | TA_TOP);
        paintSchoolMap(dc);
        for (int i = 0; i < 4; ++i) {
            const int y = px(48) + i * px(45);
            const int left = width - px(155);
            fillColor(dc, RECT{left - px(5), y - px(2), width, y + px(39)}, kBlack);
            drawTextAt(dc, smallFont_, kSchoolGirls[i].name, left, y, kWhite);
            drawSchoolHearts(dc, left, y + smallH + px(2), schoolHearts_[i]);
        }
        if (schoolHorror_.lastVictim >= 0) {
            const int victim = schoolHorror_.lastVictim;
            if (nowMs() - schoolHorror_.actors[victim].hitAt < 2200) {
                const RECT reaction{width / 2 - px(56), px(48), width / 2 + px(56), px(160)};
                drawSchoolPortrait(dc, victim, reaction);
                drawTextCentered(dc, smallFont_, kSchoolGirls[victim].name, width / 2, px(165), kWhite);
            }
        }
        const int footer = height - smallH * 2 - px(25);
        fillColor(dc, RECT{0, footer - px(5), width, height}, kBlack);
        const int target = schoolTarget();
        const bool nearKnife = !schoolHorror_.found && std::hypot(camX_ - anaise::SchoolHorror::knifeX,
            camY_ - anaise::SchoolHorror::knifeY) < 1.15;
        const std::wstring prompt = nearKnife ? L"E  TAKE THE KNIFE UNDER THE DESK"
            : nowMs() - schoolNoticeStart_ < 5000 ? schoolNotice_
            : target >= 0 ? std::wstring(L"E  TALK TO ") + kSchoolGirls[target].name
            : schoolHorror_.alarm ? L"SCREAMS ECHO THROUGH THE COLLEGE. THE EXIT HALL IS NOT EMPTY."
            : L"ART: NW   GYM: NE   LIBRARY: SW   MUSIC: SE";
        drawTextCentered(dc, smallFont_, prompt, width / 2, footer, kWhite);
        drawTextAt(dc, smallFont_, schoolHorror_.found ? L"W/S WALK  SHIFT RUN  A/D TURN  Q KNIFE  F/CLICK STAB  R RESET"
            : L"W/S WALK  SHIFT RUN  A/D TURN  E SEARCH/TALK  1/2/3 REPLY  R RESET", px(16), height - smallH - px(12), kWhite);
        paintSchoolViolence(dc, width, height);
        if (schoolTalking_ >= 0 || schoolDate_ >= 0) {
            const int girl = schoolDate_ >= 0 ? schoolDate_ : schoolTalking_;
            const int panelH = std::min(height - px(45), px(245));
            const RECT panel{px(14), height - panelH - px(12), width - px(14), height - px(12)};
            fillColor(dc, panel, kBlack); strokeRect(dc, panel, kWhite, 2);
            const int portraitSize = std::min(px(172), (width - px(60)) / 3);
            const RECT portrait{panel.left + px(14), panel.top + px(14),
                panel.left + px(14) + portraitSize, panel.top + px(14) + portraitSize};
            drawSchoolPortrait(dc, girl, portrait);
            const int textLeft = portrait.right + px(18);
            int y = panel.top + px(14);
            drawTextAt(dc, bodyFont_, kSchoolGirls[girl].name,
                       textLeft, y, kWhite);
            y += textHeight(dc, bodyFont_) + px(10);
            const int wrapW = panel.right - textLeft - px(16);
            if (schoolDate_ >= 0) {
                y = drawWrappedText(dc, bodyFont_, kSchoolGirls[girl].date, textLeft, y, wrapW, kWhite);
                drawTextAt(dc, smallFont_, L"FESTIVAL DATE UNLOCKED", textLeft, y + px(12), kWhite);
                drawTextAt(dc, smallFont_, L"ENTER RETURN TO 2D    R REPLAY SCHOOL", textLeft, panel.bottom - smallH - px(14), kWhite);
            } else {
                const bool invite = schoolRounds_[girl] >= 3;
                const auto& talk = kSchoolTalks[girl][std::min(schoolRounds_[girl], 2)];
                y = drawWrappedText(dc, bodyFont_, invite ? L"The festival starts soon. Want to make a plan?" : talk.question,
                                textLeft, y, wrapW, kWhite) + px(8);
                const wchar_t* invites[] = {L"Ask her to the festival (3 hearts)", L"Get to know her better", L"See you around"};
                for (int choice = 0; choice < 3; ++choice) {
                    y = drawWrappedText(dc, smallFont_, std::to_wstring(choice + 1) + L"  " + (invite ? invites[choice] : talk.choices[choice]),
                                    textLeft, y, wrapW, kWhite) + px(7);
                }
            }
        }
    }

    // First-person school: lockers, windows, classroom boards and club members.
    // The world DIB and its depth buffer keep every pixel binary and occluded.
    void renderCorridor(HDC dc, int width, int height) {
        if (!ensureWorldSurface(width, height)) return;
        ensureSchoolFaces();
        const std::size_t count = static_cast<std::size_t>(width) * height;
        std::fill(worldPixels_, worldPixels_ + count, 0xFF000000u);

        const double dirX = std::cos(camAngle_);
        const double dirY = std::sin(camAngle_);
        const double planeScale = std::tan(0.5 * 66.0 * kPi / 180.0);
        const double planeX = -dirY * planeScale;
        const double planeY = dirX * planeScale;
        const int horizon = height / 2 + height / 24;
        const double projection = height * 0.62;
        const double invDet = 1.0 / (planeX * dirY - dirX * planeY);
        corridorDepth_.resize(width);

        for (int x = 0; x < width; ++x) {
            const double cameraX = 2.0 * x / width - 1.0;
            const double rayX = dirX + planeX * cameraX;
            const double rayY = dirY + planeY * cameraX;
            int mapX = static_cast<int>(camX_);
            int mapY = static_cast<int>(camY_);
            const double deltaX =
                rayX == 0.0 ? 1e30 : std::abs(1.0 / rayX);
            const double deltaY =
                rayY == 0.0 ? 1e30 : std::abs(1.0 / rayY);
            const int stepX = rayX < 0 ? -1 : 1;
            const int stepY = rayY < 0 ? -1 : 1;
            double sideX = rayX < 0
                               ? (camX_ - mapX) * deltaX
                               : (mapX + 1.0 - camX_) * deltaX;
            double sideY = rayY < 0
                               ? (camY_ - mapY) * deltaY
                               : (mapY + 1.0 - camY_) * deltaY;
            int side = 0;
            double perp = 40.0;
            for (int step = 0; step < 80; ++step) {
                if (sideX < sideY) {
                    sideX += deltaX;
                    mapX += stepX;
                    side = 0;
                } else {
                    sideY += deltaY;
                    mapY += stepY;
                    side = 1;
                }
                if (corridorSolid(mapX, mapY)) {
                    perp = side == 0 ? sideX - deltaX : sideY - deltaY;
                    break;
                }
            }
            perp = std::max(0.05, perp);
            corridorDepth_[x] = perp;

            const int half = static_cast<int>(projection / perp * 0.5);
            const int top = horizon - half;
            const int bottom = horizon + half;
            const double light = std::clamp(0.80 - perp * 0.060, 0.04, 0.80);
            const double shade = side ? 0.58 : 1.0;
            const int threshold = static_cast<int>(light * shade * 64.0);
            const double hit = side == 0 ? camY_ + perp * rayY : camX_ + perp * rayX;
            const double u = hit - std::floor(hit);
            // Ceiling tiles and fluorescent panels give the halls a roof.
            for (int y = 0; y < std::min(height, top); ++y) {
                const double distance = projection * 0.5 / std::max(1, horizon - y);
                const double fx = camX_ + distance * rayX, fy = camY_ + distance * rayY;
                const double tileX = fx - std::floor(fx), tileY = fy - std::floor(fy);
                const bool panel = (static_cast<int>(std::floor(fx)) % 3 == 1) &&
                    tileX > 0.18 && tileX < 0.82 && tileY > 0.22 && tileY < 0.78;
                const bool seam = tileX < 0.025 || tileY < 0.025;
                const int ceilingShade = panel ? 42 : seam ? 9 : 0;
                if (kBayer8[y & 7][x & 7] < ceilingShade)
                    worldPixels_[static_cast<std::size_t>(y) * width + x] = 0xFFFFFFFFu;
            }
            for (int y = std::max(0, top); y < std::min(height, bottom); ++y) {
                const double v = static_cast<double>(y - top) / std::max(1, bottom - top);
                int wallShade = threshold;
                if (mapY == 9 || mapY == 15) {
                    // Hall lockers, vents, handles and panel seams.
                    if (u < 0.035 || u > 0.965 || v < 0.06 || v > 0.94) wallShade = 1;
                    if (u > 0.18 && u < 0.82 && v > 0.16 && v < 0.31 &&
                        static_cast<int>(v * 80) % 3 == 0) wallShade = 0;
                    if (u > 0.77 && u < 0.86 && v > 0.48 && v < 0.58) wallShade = 60;
                } else if ((mapY == 4 || mapY == 20) && u > 0.08 && u < 0.92 && v > 0.2 && v < 0.65) {
                    // Classroom chalkboards and their chalk marks.
                    wallShade = (v < 0.23 || v > 0.62 || u < 0.11 || u > 0.89) ? 58 :
                        ((static_cast<int>(v * 60) % 9 == 0 && u < 0.65) ? 38 : 0);
                } else if ((mapX == 0 || mapX == 30) && v > 0.15 && v < 0.68) {
                    // Tall campus windows and dark mullions.
                    wallShade = (u < 0.08 || u > 0.92 || std::abs(u - 0.5) < 0.025 || std::abs(v - 0.4) < 0.02) ? 0 : 56;
                }
                if (kBayer8[y & 7][x & 7] < wallShade) {
                    worldPixels_[static_cast<std::size_t>(y) * width + x] =
                        0xFFFFFFFFu;
                }
            }
            if (top >= 0 && top < height) {
                worldPixels_[static_cast<std::size_t>(top) * width + x] =
                    0xFFFFFFFFu;
            }
            if (bottom >= 0 && bottom < height) {
                worldPixels_[static_cast<std::size_t>(bottom) * width + x] =
                    0xFFFFFFFFu;
            }

            // The floor, cast one row at a time: a tiled grid that fades with
            // distance into dither instead of grey.
            for (int y = std::max(0, bottom); y < height; ++y) {
                const int below = y - horizon;
                if (below <= 0) continue;
                const double rowDistance = projection * 0.5 / below;
                const double stepX = rowDistance * 2.0 * planeX / width;
                const double stepY = rowDistance * 2.0 * planeY / width;
                const double fx = camX_ + rowDistance * (dirX - planeX) +
                                  stepX * x;
                const double fy = camY_ + rowDistance * (dirY - planeY) +
                                  stepY * x;
                const double gridX = fx * 2.0 - std::floor(fx * 2.0);
                const double gridY = fy * 2.0 - std::floor(fy * 2.0);
                if (gridX > 0.055 && gridY > 0.055) continue;
                const double fade =
                    std::clamp(0.95 - rowDistance * 0.075, 0.0, 0.95);
                if (kBayer8[y & 7][x & 7] < static_cast<int>(fade * 64.0)) {
                    worldPixels_[static_cast<std::size_t>(y) * width + x] =
                        0xFFFFFFFFu;
                }
            }
        }

        // Solid club members use the same wall depth buffer as the school.
        // Pools stay on the floor and behind intervening walls.
        for (const auto& pool : schoolHorror_.blood) {
            const double dx = pool.x - camX_, dy = pool.y - camY_;
            const double tx = invDet * (dirY * dx - dirX * dy);
            const double ty = invDet * (-planeY * dx + planeX * dy);
            if (ty <= .2) continue;
            const int cx = static_cast<int>(width * .5 * (1 + tx / ty));
            const int cy = horizon + static_cast<int>(projection * .5 / ty);
            const double age = static_cast<double>(nowMs() - pool.when) / 1000.0;
            const int rx = std::max(2, static_cast<int>(projection * (.22 + .15 * std::min(2.0, age)) / ty));
            const int ry = std::max(2, rx / 4);
            for (int y = std::max(0, cy - ry); y < std::min(height, cy + ry); ++y)
                for (int x = std::max(0, cx - rx); x < std::min(width, cx + rx); ++x) {
                    if (corridorDepth_[x] < ty) continue;
                    const double nx = static_cast<double>(x - cx) / rx, ny = static_cast<double>(y - cy) / ry;
                    const double edge = .82 + (glitchHash(x / 4, y / 3, pool.seed) % 18) / 100.0;
                    if (nx * nx + ny * ny < edge && kBayer8[y & 7][x & 7] < 45)
                        worldPixels_[static_cast<std::size_t>(y) * width + x] = 0xFFFFFFFFu;
                }
        }
        // The hidden knife is a small blade under the north classroom desk.
        if (!schoolHorror_.found) {
            const double dx = anaise::SchoolHorror::knifeX - camX_, dy = anaise::SchoolHorror::knifeY - camY_;
            const double tx = invDet * (dirY * dx - dirX * dy), ty = invDet * (-planeY * dx + planeX * dy);
            if (ty > .2 && ty < 8) {
                const int cx = static_cast<int>(width * .5 * (1 + tx / ty));
                const int cy = horizon + static_cast<int>(projection * .5 / ty);
                const int size = std::max(3, static_cast<int>(projection * .22 / ty));
                for (int i = 0; i < size; ++i) for (int thick = 0; thick < 3; ++thick) {
                    const int x = cx - size / 2 + i, y = cy - i / 3 - thick;
                    if (x >= 0 && x < width && y >= 0 && y < height && corridorDepth_[x] > ty)
                        worldPixels_[static_cast<std::size_t>(y) * width + x] = i < size / 3 && thick == 1 ? 0xFF000000u : 0xFFFFFFFFu;
                }
            }
        }
        // Sort back to front so nearer characters cover distant ones.
        std::array<int, 4> order{{0, 1, 2, 3}};
        std::sort(order.begin(), order.end(), [this](int a, int b) {
            return std::hypot(schoolHorror_.actors[a].x - camX_, schoolHorror_.actors[a].y - camY_) >
                   std::hypot(schoolHorror_.actors[b].x - camX_, schoolHorror_.actors[b].y - camY_);
        });
        for (int girl : order) {
            const auto& actor = schoolHorror_.actors[girl];
            if (actor.escaped) continue;
            const bool dead = actor.health == 0;
            const double dx = actor.x - camX_, dy = actor.y - camY_;
            const double tx = invDet * (dirY * dx - dirX * dy);
            const double ty = invDet * (-planeY * dx + planeX * dy);
            if (ty <= 0.25) continue;
            const int centerX = static_cast<int>(width * 0.5 * (1.0 + tx / ty));
            const int spriteH = std::max(1, static_cast<int>(projection / ty));
            const int spriteW = dead ? spriteH * 4 / 5 : spriteH * 18 / 32;
            const int drawH = dead ? std::max(1, spriteH / 4) : spriteH;
            const int bob = actor.fleeing && !dead ? static_cast<int>(std::sin(nowMs() / 85.0 + girl) * spriteH * .012) : 0;
            const int top = horizon + spriteH / 2 - drawH + bob;
            const bool photoFace = !schoolFacePixels_[girl].empty();
            const int faceSize = std::max(1, dead ? drawH : spriteH * 13 / 32);
            const int faceLeft = dead ? centerX - spriteW / 2 : centerX - faceSize / 2;
            const int expression = static_cast<int>(schoolExpression(girl));
            for (int y = std::max(0, top); y < std::min(height, top + drawH); ++y) {
                for (int x = std::max(0, centerX - spriteW / 2); x < std::min(width, centerX + spriteW / 2); ++x) {
                    if (corridorDepth_[x] <= ty) continue;
                    const int sx = dead ? (y - top) * 18 / drawH : (x - centerX + spriteW / 2) * 18 / std::max(1, spriteW);
                    const int sy = dead ? (x - centerX + spriteW / 2) * 32 / std::max(1, spriteW) : (y - top) * 32 / spriteH;
                    int ink = photoFace && sy < 13 ? -1 : schoolGirlPixel(sx, sy, girl);
                    if (actor.health < 3 && ink >= 0 && sy > 14 && sy < 26 && sx > 6 && sx < 12 &&
                        glitchHash(sx, sy, girl + actor.health) % 4 != 0) ink = 0;
                    if (photoFace && y < top + faceSize && x >= faceLeft && x < faceLeft + faceSize) {
                        const int u = (x - faceLeft) * kSchoolFaceSize / faceSize;
                        const int v = (y - top) * kSchoolFaceSize / faceSize;
                        const double nx = (u - 127.5) / 123.0, ny = (v - 127.5) / 127.5;
                        if (nx * nx + ny * ny <= 1.0) {
                            const auto pixel = schoolFacePixels_[girl][static_cast<std::size_t>(v) *
                                kSchoolFaceSize * kSchoolFaceExpressions + expression * kSchoolFaceSize + u];
                            ink = static_cast<int>(pixel & 0xFF) > kBayer8[y & 7][x & 7] * 4 + 2 ? 1 : 0;
                        }
                    }
                    if (ink >= 0) worldPixels_[static_cast<std::size_t>(y) * width + x] = ink ? 0xFFFFFFFFu : 0xFF000000u;
                }
            }
        }
        GdiFlush();
        BitBlt(dc, 0, 0, width, height, worldDc_, 0, 0, SRCCOPY);
        for (int index = 0; index < 4; ++index) {
            const auto& actor = schoolHorror_.actors[index];
            if (actor.escaped || actor.health == 0) continue;
            const double dx = actor.x - camX_, dy = actor.y - camY_;
            const double tx = invDet * (dirY * dx - dirX * dy);
            const double ty = invDet * (-planeY * dx + planeX * dy);
            if (ty <= 0.25 || ty > 12.0) continue;
            const int x = static_cast<int>(width * 0.5 * (1.0 + tx / ty));
            if (x < 0 || x >= width || corridorDepth_[x] <= ty) continue;
            const int y = std::max(px(40), horizon - static_cast<int>(projection / ty) / 2 - px(35));
            const std::wstring label = kSchoolGirls[index].name;
            const int w = textWidth(dc, smallFont_, label);
            fillColor(dc, RECT{x - w / 2 - px(4), y, x + w / 2 + px(4), y + textHeight(dc, smallFont_) + px(4)}, kBlack);
            drawTextCentered(dc, smallFont_, label, x, y + px(2), kWhite);
        }
    }

    // 18x32 casual college silhouettes beneath the detailed adult face textures.
    // -1 is transparent; 0/1 are opaque black/white.
    static int schoolGirlPixel(int x, int y, int girl) {
        if (y < 1 || y > 31) return -1;
        if (y <= 12) {
            const bool head = x >= 5 && x <= 12 && y >= 2 && y <= 10;
            const bool sideHair = y >= 3 && y <= (girl == 0 ? 10 : 15) &&
                ((x >= 3 && x <= 4) || (x >= 13 && x <= 14));
            const bool extraHair = girl == 1 ? (x >= 14 && x <= 16 && y >= 4 && y <= 9)
                : girl == 3 && (x == 2 || x == 15) && y >= 5;
            if (sideHair || extraHair) return x == 3 || x == 14 || extraHair || y == 3 ? 1 : 0;
            if (head) {
                if (y <= 4 || (y == 5 && (x + girl) % 3 != 0)) return y == 2 || x == 5 || x == 12 ? 1 : 0;
                if (y == 7 && (x == 6 || x == 11)) return 0;
                if (y == 9 && (x == 8 || x == 9)) return 0;
                return 1;
            }
            if (y >= 11 && x >= 7 && x <= 10) return 1;
            return -1;
        }
        if (y <= 21) {
            if (x < 4 || x > 13) return -1;
            if (y < 16 && (x == 4 || x == 13)) return 1;
            if (x >= 6 && x <= 11) {
                if (y < 17 && (x == y - 7 || x == 24 - y)) return 0;
                if ((x == 8 || x == 9) && y >= 15 && y <= 19) return (x + y + girl) % 2;
                return 1;
            }
            return x == 4 || x == 13 ? 1 : 0;
        }
        if (y <= 26) {
            if (x < 4 - (y - 22) / 2 || x > 13 + (y - 22) / 2) return -1;
            return y == 26 || x % 3 == 0 ? 1 : 0;
        }
        if ((x >= 5 && x <= 7) || (x >= 10 && x <= 12)) return y >= 30 ? 0 : 1;
        return -1;
    }


    void paintPainterWallpaper(HDC dc, int width, int height) const {
        const int tileWidth = px(240), tileHeight = px(82);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = tileWidth;
        info.bmiHeader.biHeight = -tileHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap || !bits) {
            if (bitmap) DeleteObject(bitmap);
            return;
        }
        HDC tile = CreateCompatibleDC(dc);
        if (!tile) {
            DeleteObject(bitmap);
            return;
        }
        HGDIOBJ old = SelectObject(tile, bitmap);
        RECT bounds{0, 0, tileWidth, tileHeight};
        fillColor(tile, bounds, kBlack);
        HFONT lettering = CreateFontW(-px(34), 0, 0, 0, FW_BOLD, FALSE,
            FALSE, FALSE, HEBREW_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_ROMAN, L"David");
        HGDIOBJ oldFont = SelectObject(tile, lettering);
        SetTextColor(tile, kWhite);
        SetBkMode(tile, TRANSPARENT);
        // Hebrew rendering of "dissolve and solidify", in logical RTL order.
        RECT text{px(12), px(6), tileWidth - px(12), px(52)};
        DrawTextW(tile, L"\u05d4\u05de\u05e1 \u05d5\u05d2\u05d1\u05e9", -1, &text,
                  DT_CENTER | DT_SINGLELINE | DT_RTLREADING | DT_NOPREFIX);
        const int cx = tileWidth / 2, y = px(65), r = px(5);
        drawLine(tile, px(42), y, cx - px(18), y);
        drawLine(tile, cx + px(18), y, tileWidth - px(42), y);
        drawLine(tile, cx - r, y, cx, y - r);
        drawLine(tile, cx, y - r, cx + r, y);
        drawLine(tile, cx + r, y, cx, y + r);
        drawLine(tile, cx, y + r, cx - r, y);
        GdiFlush();
        auto* pixels = static_cast<std::uint32_t*>(bits);
        // Cut the lettering into an etched stipple, staying purely binary.
        for (int row = 0; row < tileHeight; ++row) {
            for (int col = 0; col < tileWidth; ++col) {
                if ((col + row * 2) % 4 != 0)
                    pixels[static_cast<std::size_t>(row) * tileWidth + col] = 0;
            }
        }
        for (int row = 0, yTop = 0; yTop < height; ++row, yTop += tileHeight) {
            const int offset = (row & 1) ? tileWidth / 2 : 0;
            for (int x = -offset; x < width; x += tileWidth)
                BitBlt(dc, x, yTop, tileWidth, tileHeight, tile, 0, 0, SRCCOPY);
        }
        SelectObject(tile, oldFont);
        DeleteObject(lettering);
        SelectObject(tile, old);
        DeleteDC(tile);
        DeleteObject(bitmap);
    }

    void paintPaintScreen(HDC dc, int width, int height) {
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int logoCell = std::max(2, MulDiv(4, scale, 96));
        int ruleY = 0;
        int titleY = 0;
        paintPainterWallpaper(dc, width, height);
        fillColor(dc, RECT{0, 0, width, margin + logoCell * kLogoGrid + px(24)}, kBlack);
        drawHeader(dc, width, L"PAINTER", headingFont_, logoCell, ruleY,
                   titleY);

        const std::wstring brushStatus =
            L"BRUSH " + std::to_wstring(paintBrush_);
        const int statusHeight = textHeight(dc, smallFont_);
        drawTextAt(dc, smallFont_, brushStatus, width - margin,
                   ruleY - MulDiv(22, scale, 96) - statusHeight, kWhite,
                   TA_RIGHT | TA_TOP);

        const GridLayout layout = paintLayout(width, height);
        const int gridWidth = layout.cell * kPaintCols;
        const int gridHeight = layout.cell * kPaintRows;
        const RECT border{layout.left, layout.top, layout.left + gridWidth,
                          layout.top + gridHeight};
        // The canvas is black paper: the pattern stays out of it.
        fillColor(dc, border, kBlack);
        strokeRect(dc, border, kWhite, 1);

        const int inset = layout.cell >= 8 ? 1 : 0;
        HBRUSH whiteBrush = CreateSolidBrush(kWhite);
        for (int row = 0; row < kPaintRows; ++row) {
            for (int col = 0; col < kPaintCols; ++col) {
                if (!canvas_[row][col]) continue;
                RECT cell{layout.left + col * layout.cell + inset,
                          layout.top + row * layout.cell + inset,
                          layout.left + (col + 1) * layout.cell - inset,
                          layout.top + (row + 1) * layout.cell - inset};
                FillRect(dc, &cell, whiteBrush);
            }
        }
        DeleteObject(whiteBrush);

        if (hoverValid_) {
            const int start = -(paintBrush_ - 1) / 2;
            const int firstCol = std::max(0, hoverCol_ + start);
            const int firstRow = std::max(0, hoverRow_ + start);
            const int lastCol =
                std::min(kPaintCols - 1, hoverCol_ + start + paintBrush_ - 1);
            const int lastRow =
                std::min(kPaintRows - 1, hoverRow_ + start + paintBrush_ - 1);
            RECT hover{layout.left + firstCol * layout.cell + 1,
                       layout.top + firstRow * layout.cell + 1,
                       layout.left + (lastCol + 1) * layout.cell - 1,
                       layout.top + (lastRow + 1) * layout.cell - 1};
            strokeRect(dc, hover,
                       canvas_[hoverRow_][hoverCol_] ? kBlack : kWhite, 2);
        }
    }

    // ------------------------------------------------------------------ veil
    static int browserNoise(int x, int y, int seed) {
        unsigned int h = static_cast<unsigned int>(x) * 374761393u +
                         static_cast<unsigned int>(y) * 668265263u +
                         static_cast<unsigned int>(seed) * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return static_cast<int>(h ^ (h >> 16));
    }

    // Draws one of the engraved plates as a field of white cells. A slow band
    // eats every other dot as it passes, and fixed scanlines comb the field, so
    // the picture looks lit by a failing monitor rather than printed.
    void drawDotPlate(HDC dc, const wchar_t* const* cells, int cols,
                      int rowCount, const RECT& area, int seed) const {
        if (cols <= 0 || rowCount <= 0 || area.right <= area.left ||
            area.bottom <= area.top) {
            return;
        }
        const int pitch = std::max(
            1, std::min(static_cast<int>(area.right - area.left) / cols,
                        static_cast<int>(area.bottom - area.top) / rowCount));
        const int plateWidth = cols * pitch;
        const int plateHeight = rowCount * pitch;
        const int left = area.left + (area.right - area.left - plateWidth) / 2;
        const int top = area.top + (area.bottom - area.top - plateHeight) / 2;
        const int band =
            rowCount > 16
                ? static_cast<int>((nowMs() / 38 + seed * 11) %
                                   (rowCount + 36)) - 18
                : -100;

        HBRUSH brush = CreateSolidBrush(kWhite);
        for (int row = 0; row < rowCount; ++row) {
            const wchar_t* line = cells[row];
            if (!line) continue;
            for (int col = 0; col < cols; ++col) {
                if (line[col] != L'#') continue;
                if (std::abs(row - band) < 3 &&
                    ((col + row + seed) & 1) == 0) {
                    continue;
                }
                RECT pixel{left + col * pitch, top + row * pitch,
                           left + (col + 1) * pitch,
                           top + (row + 1) * pitch};
                FillRect(dc, &pixel, brush);
            }
        }
        DeleteObject(brush);

        HBRUSH scan = CreateSolidBrush(kBlack);
        for (int y = top; y < top + plateHeight; y += 3) {
            RECT line{left, y, left + plateWidth, y + 1};
            FillRect(dc, &line, scan);
        }
        DeleteObject(scan);
    }

    int drawWrappedText(HDC dc, HFONT font, const std::wstring& text, int x,
                        int y, int maxWidth, COLORREF color) const {
        const int height = textHeight(dc, font);
        const int lineHeight = height + std::max(2, height / 4);
        int top = y;
        std::size_t start = 0;
        while (start <= text.size()) {
            const std::size_t end = text.find(L'\n', start);
            const std::wstring paragraph =
                text.substr(start, end == std::wstring::npos
                                       ? std::wstring::npos
                                       : end - start);
            std::wstring line;
            std::size_t i = 0;
            while (i < paragraph.size()) {
                while (i < paragraph.size() && paragraph[i] == L' ') ++i;
                std::size_t j = i;
                while (j < paragraph.size() && paragraph[j] != L' ') ++j;
                if (j == i) break;
                const std::wstring word = paragraph.substr(i, j - i);
                const std::wstring candidate =
                    line.empty() ? word : line + L" " + word;
                if (!line.empty() &&
                    textWidth(dc, font, candidate) > maxWidth) {
                    drawTextAt(dc, font, line, x, top, color);
                    top += lineHeight;
                    line = word;
                } else {
                    line = candidate;
                }
                i = j;
            }
            if (!line.empty()) {
                drawTextAt(dc, font, line, x, top, color);
                top += lineHeight;
            }
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
        return top;
    }

    // A title that occasionally tears: one horizontal slice is redrawn a few
    // pixels sideways. The slice is chosen by the clock, so a fixed --time
    // always produces the same tear.
    void drawGlitchCentered(HDC dc, HFONT font, const std::wstring& text, int cx,
                            int y, int seed) const {
        drawTextCentered(dc, font, text, cx, y, kWhite);
        const int slice = static_cast<int>((nowMs() / 90 + seed * 5) % 19);
        if (slice > 2) return;
        const int height = textHeight(dc, font);
        const int width = textWidth(dc, font, text);
        if (width <= 0 || height <= 2) return;
        const int bandY = y + (slice * height) / 3;
        const int offset = slice == 0 ? std::max(2, height / 12)
                                      : -std::max(2, height / 14);
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, cx - width / 2 - 8, bandY, cx + width / 2 + 8,
                          bandY + std::max(2, height / 6));
        drawTextCentered(dc, font, text, cx + offset, y, kWhite);
        RestoreDC(dc, saved);
    }

    void drawMarquee(HDC dc, const RECT& bar, const std::wstring& message) const {
        if (bar.right <= bar.left || bar.bottom <= bar.top) return;
        fillColor(dc, bar, kWhite);
        const int textW = textWidth(dc, smallFont_, message);
        if (textW <= 0) return;
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, bar.left, bar.top, bar.right, bar.bottom);
        const int offset = static_cast<int>((nowMs() / 36) % textW);
        const int textH = textHeight(dc, smallFont_);
        const int y = bar.top + (bar.bottom - bar.top - textH) / 2;
        for (int x = bar.left - offset; x < bar.right; x += textW) {
            drawTextAt(dc, smallFont_, message, x, y, kBlack);
        }
        RestoreDC(dc, saved);
    }

    void drawProgressBar(HDC dc, const RECT& bar, double fraction,
                         bool inverted) const {
        if (bar.right <= bar.left || bar.bottom <= bar.top) return;
        fillColor(dc, bar, kBlack);
        strokeRect(dc, bar, kWhite, 1);
        const int width = static_cast<int>(
            (bar.right - bar.left - 4) * std::clamp(fraction, 0.0, 1.0));
        RECT filled{bar.left + 2, bar.top + 2, bar.left + 2 + width,
                    bar.bottom - 2};
        if (filled.right > filled.left) {
            if (inverted) {
                fillDither(dc, filled, 28);
            } else {
                fillColor(dc, filled, kWhite);
            }
        }
    }

    void drawPixelDigit(HDC dc, wchar_t digit, int left, int top,
                        int unit) const {
        static const wchar_t* const kFour[7] = {
            L"...#.", L"..##.", L".#.#.", L"#..#.", L"#####", L"...#.",
            L"...#.",
        };
        static const wchar_t* const kZero[7] = {
            L".###.", L"#...#", L"#..##", L"#.#.#", L"##..#", L"#...#",
            L".###.",
        };
        const wchar_t* const* rows = digit == L'4' ? kFour : kZero;
        HBRUSH brush = CreateSolidBrush(kWhite);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row][col] != L'#') continue;
                RECT pixel{left + col * unit, top + row * unit,
                           left + (col + 1) * unit,
                           top + (row + 1) * unit};
                FillRect(dc, &pixel, brush);
            }
        }
        DeleteObject(brush);
    }

    void drawPixel404(HDC dc, const RECT& page) const {
        const int unit = std::clamp(
            std::min(static_cast<int>(page.right - page.left) / 24,
                     static_cast<int>(page.bottom - page.top) / 16),
            4, 22);
        const int glyph = 5 * unit;
        const int gap = unit;
        const int width = glyph * 3 + gap * 2;
        const int left = (page.left + page.right - width) / 2;
        const int top = page.top + std::min(
                                       24, static_cast<int>(page.bottom - page.top) / 12);
        drawPixelDigit(dc, L'4', left, top, unit);
        drawPixelDigit(dc, L'0', left + glyph + gap, top, unit);
        drawPixelDigit(dc, L'4', left + (glyph + gap) * 2, top, unit);
    }

    void paintGamePage(HDC dc, const RECT& page, bool compact) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        RECT bar{page.left, page.top, page.right, page.top};

        const int center = (page.left + page.right) / 2;
        int y = bar.bottom + px(compact ? 12 : 22);
        HFONT big = compact ? headingFont_ : titleFont_;
        const std::wstring gameTitle = L"WELCOME TO THE GAME";
        if (textWidth(dc, big, gameTitle) <=
            page.right - page.left - px(40)) {
            drawGlitchCentered(dc, big, gameTitle, center, y, 1);
            y += textHeight(dc, big) + px(10);
        } else {
            drawGlitchCentered(dc, big, L"WELCOME", center, y, 1);
            y += textHeight(dc, big) + px(2);
            drawGlitchCentered(dc, big, L"TO THE GAME", center, y, 2);
            y += textHeight(dc, big) + px(10);
        }
        drawLine(dc, page.left + px(40), y, page.right - px(40), y);
        y += px(compact ? 8 : 12);

        const int maxTextWidth =
            std::min(static_cast<int>(page.right - page.left) - px(40),
                     px(620));
        const int textLeft = center - maxTextWidth / 2;
        if (!compact) {
            y = drawWrappedText(
                dc, bodyFont_,
                L"This is not a website. It is the place where a website used to be. You have been here before; you do not remember agreeing to it.",
                textLeft, y, maxTextWidth, kWhite);
            y += px(12);
        }

        static const wchar_t* const kRules[5] = {
            L"01   DO NOT LOOK AT THE MOON.",
            L"02   DO NOT ANSWER THE THIRD KNOCK.",
            L"03   IF YOU SEE BAEL, HE HAS ALREADY SEEN YOU.",
            L"04   DO NOT TELL ANYONE YOU ARE PLAYING.",
            L"05   THE GAME ENDS WHEN YOU STOP.",
        };
        const int boxWidth =
            std::min(static_cast<int>(page.right - page.left) - px(30),
                     px(640));
        const int smallH = textHeight(dc, smallFont_);
        const int headerH = smallH + px(8);
        int rowH = textHeight(dc, bodyFont_) + px(5);
        HFONT ruleFont = bodyFont_;
        int ruleCount = 5;
        int available = page.bottom - y;
        if (headerH + ruleCount * rowH + px(8) > available) {
            ruleFont = smallFont_;
            rowH = textHeight(dc, smallFont_) + px(4);
        }
        while (ruleCount > 1 &&
               headerH + ruleCount * rowH + px(8) > available) {
            --ruleCount;
        }
        RECT box{center - boxWidth / 2, y, center + boxWidth / 2,
                 y + headerH + ruleCount * rowH + px(8)};
        if (box.bottom > page.bottom) box.bottom = page.bottom;
        fillColor(dc, box, kBlack);
        strokeRect(dc, box, kWhite, 1);
        RECT header{box.left + 1, box.top + 1, box.right - 1,
                    box.top + headerH};
        fillColor(dc, header, kWhite);
        drawTextCentered(dc, smallFont_, L"RULES FOR THE VISITOR", center,
                         header.top + px(4), kBlack);
        int rowY = header.bottom + px(5);
        for (int i = 0; i < ruleCount; ++i) {
            drawTextAt(dc, ruleFont, kRules[i], box.left + px(10), rowY,
                       kWhite);
            rowY += rowH;
        }

    }

    void paintBaelPage(HDC dc, const RECT& page, bool compact) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const int split = page.left + (page.right - page.left) * 42 / 100;
        RECT art{page.left, page.top, split - px(18), page.bottom};
        const int captionH = textHeight(dc, smallFont_) + px(5);
        RECT plate = art;
        plate.bottom -= captionH;
        fillColor(dc, plate, kBlack);
        strokeRect(dc, plate, kWhite, 1);
        drawDotPlate(dc, anaise::kBaelArt, anaise::kBaelArtCols,
                     anaise::kBaelArtRows, plate, 1);
        drawTextCentered(dc, smallFont_, L"BAEL / KING OF THE EAST / LXVI",
                         (plate.left + plate.right) / 2,
                         plate.bottom + px(4), kWhite);

        const int textLeft = split + px(8);
        const int textWidth = page.right - textLeft;
        int y = page.top;
        HFONT big = compact ? headingFont_ : titleFont_;
        drawGlitchCentered(dc, big, L"BAEL", textLeft + textWidth / 2, y, 3);
        y += textHeight(dc, big) + px(4);
        if (!compact) {
            drawTextAt(dc, headingFont_, L"KING OF THE EAST", textLeft, y,
                       kWhite);
            y += textHeight(dc, headingFont_) + px(10);
        }
        drawLine(dc, textLeft, y, page.right, y);
        y += px(10);
        y = drawWrappedText(
            dc, bodyFont_,
            L"He comes with the head of a cat, the head of a toad, and the head of a man. All three are looking at you.",
            textLeft, y, textWidth, kWhite);
        y += px(8);
        y = drawWrappedText(
            dc, bodyFont_,
            L"He teaches the art of being forgotten. His students are very good at it.",
            textLeft, y, textWidth, kWhite);
        if (!compact) {
            y += px(8);
            y = drawWrappedText(
                dc, bodyFont_,
                L"Sixty-six legions answer him. They do not answer you.",
                textLeft, y, textWidth, kWhite);
            y += px(14);
            const int smallH = textHeight(dc, smallFont_);
            RECT facts{textLeft, y, page.right, y + smallH * 4 + px(14)};
            if (facts.bottom < page.bottom - px(28)) {
                fillColor(dc, facts, kBlack);
                strokeRect(dc, facts, kWhite, 1);
                int factY = facts.top + px(6);
                drawTextAt(dc, smallFont_, L"RANK      KING", textLeft + px(8),
                           factY, kWhite);
                factY += smallH;
                drawTextAt(dc, smallFont_, L"LEGIONS   66", textLeft + px(8),
                           factY, kWhite);
                factY += smallH;
                drawTextAt(dc, smallFont_, L"VOICE     HOARSE",
                           textLeft + px(8), factY, kWhite);
                factY += smallH;
                drawTextAt(dc, smallFont_, L"STATUS    PRESENT",
                           textLeft + px(8), factY, kWhite);
            }
        }
    }

    void paintOrderPage(HDC dc, const RECT& page, bool compact) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const int split = page.left + (page.right - page.left) * 58 / 100;
        RECT bar{page.left, page.top, split - px(12), page.top};
        int y = bar.bottom + px(compact ? 8 : 14);
        HFONT big = compact ? headingFont_ : titleFont_;
        drawGlitchCentered(dc, big, L"JOIN US", page.left + (split - px(12) - page.left) / 2,
                           y, 4);
        y += textHeight(dc, big) + px(4);
        const int columnWidth = split - px(12) - page.left;
        y = drawWrappedText(
            dc, smallFont_,
            L"EST. 1666   //   STILL ACCEPTING APPLICATIONS FROM THE DEAD",
            page.left, y, columnWidth, kWhite);
        y += px(compact ? 6 : 10);
        y = drawWrappedText(
            dc, bodyFont_,
            compact ? L"We meet in a room with no windows. If you are meant to find us, you will wake there."
                    : L"We meet on the thirteenth night of every month, in a room with no windows. There is no address. If you are meant to find us, you will wake there.",
            page.left, y, columnWidth, kWhite);
        y += px(compact ? 6 : 12);

        const int smallH = textHeight(dc, smallFont_);
        RECT form{page.left, y, split - px(12),
                  std::min(static_cast<int>(page.bottom) - smallH - px(8),
                           y + px(compact ? 88 : 122))};
        if (form.bottom > form.top + px(24)) {
            fillColor(dc, form, kBlack);
            strokeRect(dc, form, kWhite, 1);
            RECT header{form.left + 1, form.top + 1, form.right - 1,
                        form.top + smallH + px(7)};
            fillColor(dc, header, kWhite);
            drawTextCentered(dc, smallFont_, L"APPLICATION FOR INITIATION",
                             (form.left + form.right) / 2,
                             header.top + px(3), kBlack);
            int fieldY = header.bottom + px(5);
            const wchar_t* fields[3] = {
                L"NAME ............. YOU",
                L"ADDRESS .......... YOU",
                L"REASON ........... YOU",
            };
            for (const wchar_t* field : fields) {
                if (fieldY + smallH > form.bottom - px(4)) break;
                drawTextAt(dc, smallFont_, field, form.left + px(8), fieldY,
                           kWhite);
                fieldY += smallH + px(3);
            }
            const int buttonW = px(58);
            const int buttonH = smallH + px(6);
            RECT button{form.right - px(8) - buttonW,
                        form.bottom - px(6) - buttonH,
                        form.right - px(8), form.bottom - px(6)};
            if (button.top > fieldY - px(2)) {
                fillColor(dc, button, kWhite);
                drawTextCentered(dc, smallFont_, L"SUBMIT",
                                 (button.left + button.right) / 2,
                                 button.top + px(3), kBlack);
            }
        }

        RECT art{page.left + (page.right - page.left) * 57 / 100,
                 page.top + px(24), page.right,
                 page.top + (page.bottom - page.top) * 78 / 100};
        const int captionH = smallH + px(4);
        RECT plate = art;
        plate.bottom -= captionH;
        fillColor(dc, plate, kBlack);
        strokeRect(dc, plate, kWhite, 1);
        drawDotPlate(dc, anaise::kSabbatArt, anaise::kSabbatArtCols,
                     anaise::kSabbatArtRows, plate, 5);
        drawTextCentered(dc, smallFont_, L"FIG. 03 / THE CONJURATION",
                         (plate.left + plate.right) / 2,
                         plate.bottom + px(3), kWhite);
        drawTextAt(dc, smallFont_, L"WE DO NOT REPLY. WE ARRIVE.", page.left,
                   page.bottom - smallH, kWhite);
    }

    void paintGoatPage(HDC dc, const RECT& page, bool compact) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const int split = page.left + (page.right - page.left) * 40 / 100;
        RECT art{page.left, page.top, split - px(16), page.bottom};
        const int captionH = textHeight(dc, smallFont_) + px(5);
        RECT plate = art;
        plate.bottom -= captionH;
        fillColor(dc, plate, kBlack);
        strokeRect(dc, plate, kWhite, 1);
        drawDotPlate(dc, anaise::kBaphometArt, anaise::kBaphometArtCols,
                     anaise::kBaphometArtRows, plate, 7);
        drawTextCentered(dc, smallFont_, L"FIG. 13 / THE GOAT OF MENDES",
                         (plate.left + plate.right) / 2,
                         plate.bottom + px(4), kWhite);

        const int textLeft = split + px(6);
        const int textWidth = page.right - textLeft;
        int y = page.top;
        HFONT big = compact ? headingFont_ : titleFont_;
        drawGlitchCentered(dc, big, L"THE SABBATIC GOAT",
                           textLeft + textWidth / 2, y, 6);
        y += textHeight(dc, big) + px(4);
        if (!compact) {
            drawTextAt(dc, headingFont_, L"SOLVE ET COAGULA", textLeft, y,
                       kWhite);
            y += textHeight(dc, headingFont_) + px(10);
        }
        drawLine(dc, textLeft, y, page.right, y);
        y += px(10);
        y = drawWrappedText(
            dc, bodyFont_,
            L"The goat of Mendes sits on the world and points two ways at once.",
            textLeft, y, textWidth, kWhite);
        y += px(8);
        y = drawWrappedText(
            dc, bodyFont_,
            L"Its torch is the only light permitted at the altar. It casts no shadow because there is nothing behind it.",
            textLeft, y, textWidth, kWhite);
        if (!compact) {
            y += px(8);
            y = drawWrappedText(
                dc, bodyFont_,
                L"The pentagram on its brow is not a star. It is an opening.",
                textLeft, y, textWidth, kWhite);
            y += px(12);
            drawTextAt(dc, smallFont_,
                       L"solve.coagula(you);   //   RETURN VALUE: VOID",
                       textLeft, y, kWhite);
        }
    }

    void paint404Page(HDC dc, const RECT& page, bool compact) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        drawPixel404(dc, page);
        const int center = (page.left + page.right) / 2;
        const int smallH = textHeight(dc, smallFont_);
        int y = page.top +
                std::max(px(96), static_cast<int>(page.bottom - page.top) / 2 -
                                     px(compact ? 30 : 54));
        drawGlitchCentered(dc, compact ? headingFont_ : titleFont_,
                           L"THE PAGE THAT WAS NEVER THERE", center, y, 8);
        y += textHeight(dc, compact ? headingFont_ : titleFont_) + px(10);
        const int maxWidth =
            std::min(static_cast<int>(page.right - page.left) - px(40),
                     px(660));
        drawTextCentered(dc, smallFont_, L"ERROR 404", center, y, kWhite);
        y += smallH + px(8);
        y = drawWrappedText(
            dc, bodyFont_,
            L"You followed a link that does not exist. There is no server at this address. There has never been a server at this address.",
            center - maxWidth / 2, y, maxWidth, kWhite);
        y += px(8);
        if (!compact) {
            y = drawWrappedText(
                dc, bodyFont_,
                L"If this page is still loading, close the window. If it has already loaded, do not.",
                center - maxWidth / 2, y, maxWidth, kWhite);
            y += px(12);
        }
        const int barWidth =
            std::min(static_cast<int>(page.right - page.left) - px(80),
                     px(420));
        RECT progress{center - barWidth / 2, y, center + barWidth / 2,
                      y + px(12)};
        const double pulse =
            0.985 + 0.015 * (0.5 + 0.5 * std::sin(nowMs() * 0.002));
        drawProgressBar(dc, progress, pulse, false);
        y = progress.bottom + px(5);
        drawTextCentered(dc, smallFont_, L"RETRYING  //  99%", center, y,
                         kWhite);
        drawTextAt(dc, smallFont_,
                   L"ERR_EMPTY_TOMB   //   THE HOST ANSWERED IN YOUR VOICE",
                   page.left, page.bottom - smallH, kWhite);
    }

    // ---------------------------------------------------------- veil pages
    // The plates, marquees and hit counters below are the shared furniture of
    // the dead web. Each site is a row in kSites; only the copy and the plate
    // change from address to address.
    int px(int value) const {
        return MulDiv(value, static_cast<int>(dpi_), 96);
    }

    int drawParagraphs(HDC dc, HFONT font, const wchar_t* text, int x, int y,
                       int maxWidth, int bottom, int gap) const {
        if (!text || !*text) return y;
        const std::wstring all(text);
        const int lineH = textHeight(dc, font);
        std::size_t start = 0;
        while (start <= all.size()) {
            const std::size_t end = all.find(L'\n', start);
            const std::wstring paragraph =
                all.substr(start, end == std::wstring::npos
                                      ? std::wstring::npos
                                      : end - start);
            if (!paragraph.empty()) {
                if (y + lineH > bottom) break;
                y = drawWrappedText(dc, font, paragraph, x, y, maxWidth,
                                    kWhite);
                y += gap;
            }
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
        return y;
    }

    int factBoxHeight(HDC dc, const wchar_t* facts) const {
        if (!facts || !*facts) return 0;
        int rows = 1;
        for (const wchar_t* p = facts; *p; ++p) {
            if (*p == L'\n') ++rows;
        }
        const int smallH = textHeight(dc, smallFont_);
        return smallH + px(10) + rows * (smallH + px(3)) + px(6);
    }

    void drawFactBox(HDC dc, const wchar_t* facts, const RECT& box) const {
        if (!facts || !*facts || box.bottom <= box.top) return;
        const int smallH = textHeight(dc, smallFont_);
        fillColor(dc, box, kBlack);
        strokeRect(dc, box, kWhite, 1);
        RECT header{box.left + 1, box.top + 1, box.right - 1,
                    box.top + smallH + px(7)};
        fillColor(dc, header, kWhite);
        drawTextCentered(dc, smallFont_, L"SITE FACTS",
                         (box.left + box.right) / 2, header.top + px(3),
                         kBlack);
        int y = header.bottom + px(5);
        const std::wstring all(facts);
        std::size_t start = 0;
        while (start <= all.size()) {
            const std::size_t end = all.find(L'\n', start);
            const std::wstring row =
                all.substr(start, end == std::wstring::npos
                                      ? std::wstring::npos
                                      : end - start);
            const std::size_t bar = row.find(L'|');
            if (bar != std::wstring::npos && y + smallH <= box.bottom - px(2)) {
                const std::wstring label = row.substr(0, bar);
                const std::wstring value = row.substr(bar + 1);
                drawTextAt(dc, smallFont_, label, box.left + px(8), y, kWhite);
                drawTextAt(dc, smallFont_, value, box.right - px(8), y,
                           kWhite, TA_RIGHT | TA_TOP);
                const int dotLeft = box.left + px(8) +
                                    textWidth(dc, smallFont_, label) + px(4);
                const int dotRight = box.right - px(8) -
                                     textWidth(dc, smallFont_, value) - px(4);
                for (int x = dotLeft; x < dotRight; x += px(4)) {
                    SetPixelV(dc, x, y + smallH - px(3), kWhite);
                }
                y += smallH + px(3);
            }
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
    }

    void drawWebBadge(HDC dc, int left, int top, int width,
                      const wchar_t* label) const {
        const int height = px(28);
        if (width <= px(20)) return;
        RECT box{left, top, left + width, top + height};
        fillColor(dc, box, kBlack);
        strokeRect(dc, box, kWhite, 1);
        RECT inner{left + 2, top + 2, left + width - 2, top + height - 2};
        strokeRect(dc, inner, kWhite, 1);
        RECT mark{inner.right - px(9), inner.top, inner.right,
                  inner.top + px(9)};
        fillDither(dc, mark, 22);
        const int textH = textHeight(dc, smallFont_);
        drawTextCentered(dc, smallFont_, label,
                         (left + left + width) / 2,
                         top + (height - textH) / 2, kWhite);
    }

    void drawHitCounter(HDC dc, int x, int y, int value, int digits) const {
        const int smallH = textHeight(dc, smallFont_);
        const int boxW = std::max(px(9), smallH * 3 / 4);
        const int boxH = smallH + px(4);
        drawTextAt(dc, smallFont_, L"VISITORS", x, y, kWhite);
        int left = x + textWidth(dc, smallFont_, L"VISITORS") + px(5);
        std::wstring number = std::to_wstring(std::max(0, value));
        while (static_cast<int>(number.size()) < digits) {
            number.insert(number.begin(), L'0');
        }
        if (static_cast<int>(number.size()) > digits) {
            number = number.substr(number.size() - digits);
        }
        for (wchar_t digit : number) {
            RECT box{left, y, left + boxW, y + boxH};
            fillColor(dc, box, kWhite);
            strokeRect(dc, box, kWhite, 1);
            drawTextCentered(dc, smallFont_, std::wstring(1, digit),
                             left + boxW / 2,
                             y + (boxH - smallH) / 2, kBlack);
            left += boxW + px(1);
        }
    }

    void drawPlateWithCaption(HDC dc, const RECT& area,
                              const wchar_t* const* art, int cols, int rows,
                              const wchar_t* caption, int seed) const {
        if (!art || cols <= 0 || rows <= 0) return;
        const int captionH = textHeight(dc, smallFont_) + px(5);
        RECT plate = area;
        plate.bottom -= captionH;
        if (plate.right <= plate.left + px(8) ||
            plate.bottom <= plate.top + px(8)) {
            return;
        }
        fillColor(dc, plate, kBlack);
        strokeRect(dc, plate, kWhite, 1);
        drawDotPlate(dc, art, cols, rows, plate, seed);
        drawTextCentered(dc, smallFont_, caption,
                         (plate.left + plate.right) / 2,
                         plate.bottom + px(3), kWhite);
    }

    void paintSitePage(HDC dc, const RECT& page, bool compact,
                       const VeilSite& site) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        RECT bar{page.left, page.top, page.right, page.top};

        const bool showArt = site.art && !compact &&
                             page.bottom - page.top >= px(300);
        const int pageWidth = page.right - page.left;
        const int split =
            page.left + pageWidth * (site.artSide == 1 ? 42 : 58) / 100;
        int textLeft = page.left;
        int textRight = page.right;
        if (showArt) {
            if (site.artSide == 1) {
                textLeft = split + px(14);
            } else {
                textRight = split - px(14);
            }
        }
        const int columnWidth = std::max(px(80), textRight - textLeft);

        int y = bar.bottom + px(compact ? 8 : 12);
        const int smallH = textHeight(dc, smallFont_);

        const int headingW =
            textWidth(dc, headingFont_, site.heading);
        if (headingW <= columnWidth) {
            drawGlitchCentered(dc, headingFont_, site.heading,
                               textLeft + columnWidth / 2, y,
                               browserPage_ + 1);
            y += textHeight(dc, headingFont_) + px(2);
        } else {
            y = drawWrappedText(dc, headingFont_, site.heading, textLeft, y,
                                columnWidth, kWhite);
            y += px(2);
        }
        drawTextAt(dc, compact ? smallFont_ : bodyFont_, site.subheading,
                   textLeft, y, kWhite);
        y += (compact ? smallH : textHeight(dc, bodyFont_)) + px(6);
        drawLine(dc, textLeft, y, textRight, y);
        y += px(8);

        const int footerLineH = smallH + px(3);
        const int footerTop = page.bottom - footerLineH * 2 - px(2);
        const bool showChrome = !compact && page.bottom - page.top >= px(330);
        const int badgeH = showChrome ? px(28) : 0;
        const int factsH = showChrome ? factBoxHeight(dc, site.facts) : 0;
        int contentBottom = page.bottom - px(6);
        if (showChrome) contentBottom = footerTop - px(3) - badgeH;
        int factsTop = 0;
        if (factsH > 0) {
            factsTop = contentBottom - factsH;
            contentBottom = factsTop - px(6);
        }
        const int bodyBottom = std::max(y, contentBottom);

        const int saved = SaveDC(dc);
        IntersectClipRect(dc, textLeft, y, textRight, bodyBottom);
        y = drawParagraphs(dc, compact ? smallFont_ : bodyFont_, site.body,
                           textLeft, y, columnWidth, bodyBottom,
                           px(compact ? 2 : 6));
        RestoreDC(dc, saved);

        if (factsH > 0 && factsTop > y + px(2)) {
            RECT facts{textLeft, factsTop, textRight, factsTop + factsH};
            drawFactBox(dc, site.facts, facts);
        }
        if (showArt) {
            const int artLeft =
                site.artSide == 1 ? page.left : split + px(8);
            const int artRight =
                site.artSide == 1 ? split - px(14) : page.right;
            RECT art{artLeft, bar.bottom + px(8), artRight,
                     page.bottom - px(2)};
            drawPlateWithCaption(dc, art, site.art, site.artCols,
                                 site.artRows, site.caption,
                                 browserPage_ + 11);
        }
    }

    void paintGuestbookPage(HDC dc, const RECT& page, bool compact) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const VeilSite& site = kSites[browserPage_];
        RECT bar{page.left, page.top, page.right, page.top};

        const bool showArt =
            !compact && page.bottom - page.top >= px(300);
        const int split = page.left + (page.right - page.left) * 68 / 100;
        const int textRight = showArt ? split - px(14) : page.right;
        const int smallH = textHeight(dc, smallFont_);
        int y = bar.bottom + px(compact ? 8 : 12);
        drawGlitchCentered(dc, headingFont_, site.heading,
                           (page.left + textRight) / 2, y, 7);
        y += textHeight(dc, headingFont_) + px(2);
        drawTextAt(dc, compact ? smallFont_ : bodyFont_, site.subheading,
                   page.left, y, kWhite);
        y += smallH + px(6);
        drawLine(dc, page.left, y, textRight, y);
        y += px(8);

        static const wchar_t* const kEntries[] = {
            L"013 // 06.06.1966 // \"I only came for the picture.\" -- ANON",
            L"012 // 06.06.1966 // \"The goat blinked first.\" -- M.",
            L"011 // 06.06.1966 // \"I signed this before I was born.\" -- YOU",
            L"010 // 06.06.1966 // \"Do not click the next link.\" -- [DELETED]",
            L"009 // 06.06.1966 // \"The page is still loading.\" -- ANON",
        };
        const int entryCount = compact ? 3 : 5;
        for (int i = 0; i < entryCount; ++i) {
            y = drawWrappedText(dc, smallFont_, kEntries[i], page.left, y,
                                textRight - page.left, kWhite);
            drawLine(dc, page.left, y + px(2), textRight, y + px(2));
            y += px(7);
        }

        const int formTop = page.bottom - px(compact ? 46 : 64);
        if (formTop > y + px(4)) {
            RECT form{page.left, formTop, textRight, page.bottom - px(2)};
            fillColor(dc, form, kBlack);
            strokeRect(dc, form, kWhite, 1);
            RECT header{form.left + 1, form.top + 1, form.right - 1,
                        form.top + smallH + px(7)};
            fillColor(dc, header, kWhite);
            drawTextCentered(dc, smallFont_, L"SIGN THE GUESTBOOK",
                             (form.left + form.right) / 2,
                             header.top + px(3), kBlack);
            int fieldY = header.bottom + px(4);
            drawTextAt(dc, smallFont_, L"NAME ................ YOU",
                       form.left + px(8), fieldY, kWhite);
            fieldY += smallH + px(2);
            drawTextAt(dc, smallFont_, L"COMMENT ....... (ALREADY WRITTEN)",
                       form.left + px(8), fieldY, kWhite);
            const int buttonW = px(54);
            const int buttonH = smallH + px(4);
            RECT button{form.right - px(8) - buttonW,
                        form.bottom - px(5) - buttonH,
                        form.right - px(8), form.bottom - px(5)};
            if (button.top > fieldY) {
                fillColor(dc, button, kWhite);
                drawTextCentered(dc, smallFont_, L"SIGN",
                                 (button.left + button.right) / 2,
                                 button.top + px(2), kBlack);
            }
        }
        drawTextAt(dc, smallFont_, site.footer, page.left,
                   page.bottom - smallH, kWhite);

        if (showArt) {
            RECT art{split + px(8), bar.bottom + px(8), page.right,
                     page.bottom - px(2)};
            drawPlateWithCaption(dc, art, site.art, site.artCols,
                                 site.artRows, site.caption,
                                 browserPage_ + 5);
        }
    }

    void paintBrowserLoading(HDC dc, const BrowserLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& content = layout.content;
        fillColor(dc, content, kBlack);
        fillDither(dc, content, 3);
        const int center = (content.left + content.right) / 2;
        const int y = content.top + (content.bottom - content.top) / 3;
        drawTextCentered(dc, headingFont_, L"Loading", center, y,
                         kWhite);
        const std::wstring host =
            std::wstring(L"~ ") + kSites[browserPage_].host + L" ~";
        drawTextCentered(dc, bodyFont_, host, center,
                         y + textHeight(dc, headingFont_) + px(8), kWhite);
        const ULONGLONG elapsed = nowMs() - browserLoadStart_;
        const double fraction =
            std::clamp(static_cast<double>(elapsed) / 720.0, 0.0, 1.0);
        const int width =
            std::min(static_cast<int>(content.right - content.left) - px(80),
                     px(460));
        RECT bar{center - width / 2, y + px(92), center + width / 2,
                 y + px(104)};
        drawProgressBar(dc, bar, fraction, false);
    }

    void paintBrowserLink(HDC dc, const BrowserLayout& layout) const {
        const RECT link = browserLinkRect(layout);
        const bool hot = browserHover_ == kBrowserHitLink;
        if (hot) {
            fillColor(dc, link, kWhite);
            drawTextCentered(dc, smallFont_, kSites[browserPage_].link,
                             (link.left + link.right) / 2,
                             link.top + (link.bottom - link.top -
                                         textHeight(dc, smallFont_)) /
                                            2,
                             kBlack);
        } else {
            strokeRect(dc, link, kWhite, 1);
            drawTextCentered(dc, smallFont_, kSites[browserPage_].link,
                             (link.left + link.right) / 2,
                             link.top + (link.bottom - link.top -
                                         textHeight(dc, smallFont_)) /
                                            2,
                             kWhite);
        }
    }

    static constexpr int browserFaceResource(int seed) {
        return 303 + ((seed % 4) + 4) % 4;
    }

    void drawBrowserLens(HDC dc, HDC source, const RECT& dest,
                         int sourceWidth, int sourceHeight, int seed) {
        const int w = dest.right - dest.left;
        const int h = dest.bottom - dest.top;
        if (w <= 0 || h <= 0) return;
        // One slow downward pass, then a rest. Smoothstep eases the glass in
        // and out beyond the photograph; inverse mapping preserves 1-bit art.
        const double phase = static_cast<double>((nowMs() + (seed % 4) * 1100) % 9600);
        if (phase >= 6800.0) {
            StretchBlt(dc, dest.left, dest.top, w, h, source, 0, 0,
                sourceWidth, sourceHeight, SRCCOPY);
            return;
        }
        const double travel = phase / 6800.0;
        const double center = (-.30 + 1.60 * travel) * h;
        const double radius = std::max(1.0, h * .27);
        for (int y = 0; y < h; ++y) {
            const double distance = std::abs(y - center) / radius;
            const double falloff = std::max(0.0, 1.0 - distance);
            const double smooth = falloff * falloff * (3.0 - 2.0 * falloff);
            const double magnify = 1.0 + .62 * smooth;
            const int sampleWidth = std::max(1, static_cast<int>(sourceWidth / magnify));
            const int sourceX = (sourceWidth - sampleWidth) / 2;
            const double warpedY = center + (y - center) / magnify;
            const int sourceY = std::clamp(static_cast<int>(warpedY * sourceHeight / h), 0, sourceHeight - 1);
            StretchBlt(dc, dest.left, dest.top + y, w, 1, source,
                sourceX, sourceY, sampleWidth, 1, SRCCOPY);
        }
    }

    void drawBrowserWatcher(HDC dc, int cx, int cy, int size, int seed) {
        // Distinct licensed portraits, with a travelling lens in the bitmap.
        HBITMAP bitmap = LoadBitmapW(instance_, MAKEINTRESOURCEW(browserFaceResource(seed)));
        if (!bitmap) return;
        HDC source = CreateCompatibleDC(dc);
        const auto old = SelectObject(source, bitmap);
        const auto oldText = SetTextColor(dc, kBlack);
        const auto oldBackground = SetBkColor(dc, kWhite);
        SetStretchBltMode(dc, COLORONCOLOR);
        const int w = size * 8 / 5;
        const int shift = browserHover_ < 0 ? 0 : px(browserHover_ % 3 - 1);
        const int left = cx - w / 2 + shift;
        drawBrowserLens(dc, source, RECT{left, cy - size, left + w, cy + size}, 256, 320, seed);
        SetTextColor(dc, oldText); SetBkColor(dc, oldBackground);
        SelectObject(source, old); DeleteDC(source); DeleteObject(bitmap);
    }

    void paintBrowserPanel(HDC dc, const BrowserLayout& layout) {
        const auto rows = browserRows();
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, layout.content.left, layout.content.top, layout.content.right, layout.content.bottom);
        const wchar_t* title = browserPanel_ == 2 ? L"HISTORY" :
            browserPanel_ == 3 ? L"BOOKMARKS" : L"DIRECTORY";
        drawTextAt(dc, bodyFont_, title, layout.content.left + px(16), layout.content.top + px(12), kWhite);
        drawTextAt(dc, smallFont_, browserQuery_.empty() ? L"" :
            L"SEARCH: " + browserQuery_, layout.content.left + px(16), layout.content.top + px(38), kWhite);
        if (rows.empty()) {
            drawTextAt(dc, bodyFont_, browserPanel_ == 3 ? L"No bookmarks" : L"No results", layout.content.left + px(16), layout.content.top + px(80), kWhite);
        }
        const int rowClip = SaveDC(dc);
        IntersectClipRect(dc, layout.content.left, layout.content.top + px(66), layout.content.right, layout.content.bottom);
        for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
            const int y = layout.content.top + px(66) + i * px(30) - browserPanelScroll_;
            if (y + px(30) <= layout.content.top + px(66) || y >= layout.content.bottom) continue;
            RECT r{layout.content.left + px(12), y, layout.content.right - px(16), y + px(28)};
            bool hot = browserHover_ == 100 + i;
            if (hot) fillColor(dc, r, kWhite);
            drawTextAt(dc, smallFont_, std::to_wstring(i + 1) + L"  " + kSites[rows[i]].tab + L"   /   " + kSites[rows[i]].url,
                r.left + px(5), y + px(6), hot ? kBlack : kWhite);
            drawLine(dc, r.left, r.bottom, r.right, r.bottom);
        }
        RestoreDC(dc, rowClip);
        RestoreDC(dc, saved);
    }

    void paintBrowserRemains(HDC dc, const RECT& page) {
        const int top = page.bottom + px(24);
        const int width = page.right - page.left;
        drawLine(dc, page.left, top, page.right, top);
        RECT photo{page.left, top + px(12), page.left + width * 2 / 3, top + px(272)};
        HBITMAP bitmap = LoadBitmapW(instance_, MAKEINTRESOURCEW(browserPage_ % 2 ? 302 : 301));
        if (bitmap) {
            HDC source = CreateCompatibleDC(dc);
            auto old = SelectObject(source, bitmap);
            const auto oldText = SetTextColor(dc, kBlack);
            const auto oldBackground = SetBkColor(dc, kWhite);
            SetStretchBltMode(dc, COLORONCOLOR);
            drawBrowserLens(dc, source, photo, 640, 360, browserPage_);
            SetTextColor(dc, oldText); SetBkColor(dc, oldBackground);
            SelectObject(source, old); DeleteDC(source); DeleteObject(bitmap);
        }
        // An impossible occupant appears in each host's cached room.
        const int figureX = photo.left + (photo.right - photo.left) * (3 + browserPage_ % 5) / 10;
        const int figureY = photo.top + px(100);
        fillColor(dc, RECT{figureX - px(14), figureY, figureX + px(14), photo.bottom}, kBlack);
        drawBrowserWatcher(dc, figureX, figureY, px(31), browserPage_ + 33);
        strokeRect(dc, photo, kWhite, 1);
        const int faceX = photo.right + (page.right - photo.right) / 2;
        drawBrowserWatcher(dc, faceX, top + px(135), px(75), browserPage_ + 71);
    }

    void paintBrowser(HDC dc, int width, int height) {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const BrowserLayout layout = browserLayout(width, height);
        fillColor(dc, layout.window, kBlack);
        strokeRect(dc, layout.window, kWhite, 1);

        // Independent tabs retain their own navigation stack and page position.
        for (int i = 0; i < static_cast<int>(browserTabs_.size()); ++i) {
            const RECT& tab = layout.tabs[i];
            if (i == browserActive_) {
                fillColor(dc, tab, kWhite);
            } else {
                fillColor(dc, tab, kBlack);
                strokeRect(dc, tab, kWhite, 1);
                if (browserHover_ == i) fillDither(dc, tab, 14);
            }
            const int tabH = textHeight(dc, smallFont_);
            const int saved = SaveDC(dc);
            IntersectClipRect(dc, tab.left + px(2), tab.top, tab.right - px(2),
                              tab.bottom);
            drawTextCentered(dc, smallFont_, std::wstring(kSites[browserTabs_[i].history[browserTabs_[i].cursor]].tab) + L"    x",
                             (tab.left + tab.right) / 2,
                             tab.top + (tab.bottom - tab.top - tabH) / 2,
                             i == browserActive_ ? kBlack : kWhite);
            RestoreDC(dc, saved);
        }
        drawLine(dc, layout.window.left, layout.tabsBottom,
                 layout.window.right, layout.tabsBottom);

        auto plate = [&](const RECT& r, const wchar_t* label, bool selected) {
            fillColor(dc, r, selected ? kWhite : kBlack);
            strokeRect(dc, r, kWhite, 1);
            drawTextCentered(dc, smallFont_, label, (r.left + r.right) / 2,
                r.top + (r.bottom - r.top - textHeight(dc, smallFont_)) / 2, selected ? kBlack : kWhite);
        };
        plate(layout.newTab, L"+", false);
        plate(layout.directory, L"DIRECTORY", browserPanel_ == 1);
        plate(layout.history, L"HISTORY", browserPanel_ == 2);
        plate(layout.bookmarks, L"BOOKMARKS", browserPanel_ == 3);
        plate(layout.star, browserBookmarks_[browserPage_] ? L"*" : L"+", browserBookmarks_[browserPage_]);

        // Back, forward and reload.
        auto drawArrow = [&](const RECT& boxRect, bool forward, bool hot) {
            if (hot) fillDither(dc, boxRect, 18);
            strokeRect(dc, boxRect, kWhite, 1);
            const int cx = (boxRect.left + boxRect.right) / 2;
            const int cy = (boxRect.top + boxRect.bottom) / 2;
            const int reach = std::max(
                2, static_cast<int>(boxRect.right - boxRect.left) / 4);
            const int dir = forward ? 1 : -1;
            drawLine(dc, cx - dir * reach, cy, cx + dir * reach, cy);
            drawLine(dc, cx + dir * reach, cy, cx, cy - reach);
            drawLine(dc, cx + dir * reach, cy, cx, cy + reach);
        };
        drawArrow(layout.back, false, browserHover_ == kBrowserHitBack);
        drawArrow(layout.forward, true, browserHover_ == kBrowserHitForward);
        if (browserHover_ == kBrowserHitReload) {
            fillDither(dc, layout.reload, 18);
        }
        strokeRect(dc, layout.reload, kWhite, 1);
        {
            const int cx = (layout.reload.left + layout.reload.right) / 2;
            const int cy = (layout.reload.top + layout.reload.bottom) / 2;
            const int radius = std::max(
                3, (static_cast<int>(layout.reload.right - layout.reload.left)) /
                       3);
            HPEN pen = CreatePen(PS_SOLID, 1, kWhite);
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
            drawLine(dc, cx + radius - 2, cy - radius - 1, cx + radius + 2,
                     cy - radius + 3);
        }

        // Address field.
        fillColor(dc, layout.address, kBlack);
        strokeRect(dc, layout.address, kWhite, 1);
        const std::wstring addressText =
            (browserEditing_ ? browserAddress_ + (nowMs() / 500 % 2 ? L"_" : L"|") : std::wstring(kSites[browserPage_].url));
        const int addressH = textHeight(dc, smallFont_);
        const int addressClip = SaveDC(dc);
        IntersectClipRect(dc, layout.address.left + px(2), layout.address.top + 1, layout.address.right - px(2), layout.address.bottom - 1);
        if (browserEditing_ && browserSelectAll_) {
            SetBkMode(dc, OPAQUE); SetBkColor(dc, kWhite);
        }
        drawTextAt(dc, smallFont_, addressText, layout.address.left + px(6),
                   layout.address.top +
                       (layout.address.bottom - layout.address.top - addressH) / 2,
                   browserEditing_ && browserSelectAll_ ? kBlack : kWhite);
        RestoreDC(dc, addressClip);

        // Page.
        fillColor(dc, layout.content, kBlack);
        if (browserPanel_) {
            paintBrowserPanel(dc, layout);
        } else if (browserLoading()) {
            paintBrowserLoading(dc, layout);
        } else {
            RECT page = layout.content;
            InflateRect(&page, -px(20), -px(18));
            const RECT link = browserLinkRect(layout);
            page.bottom = link.top - px(8);
            const int viewport = SaveDC(dc);
            IntersectClipRect(dc, layout.content.left, layout.content.top, layout.content.right, link.top - px(4));
            SetViewportOrgEx(dc, 0, -browserTabs_[browserActive_].scroll, nullptr);
            const bool compact = page.bottom - page.top < px(300);
            switch (browserPage_) {
                case 0: paintGamePage(dc, page, compact); break;
                case 1: paintBaelPage(dc, page, compact); break;
                case 2: paintOrderPage(dc, page, compact); break;
                case 3: paintGoatPage(dc, page, compact); break;
                case 12: paintGuestbookPage(dc, page, compact); break;
                case 14: paint404Page(dc, page, compact); break;
                default:
                    paintSitePage(dc, page, compact, kSites[browserPage_]);
                    break;
            }
            // Slow, deterministic intrusions inside the document. Browser
            // controls remain available, including during a presence event.
            if ((nowMs() / 1000) % 17 >= 4 && (nowMs() / 1000) % 17 <= 7) {
                RECT witness{page.right - px(142), page.top + px(56), page.right, page.top + px(222)};
                fillColor(dc, witness, kBlack);
                strokeRect(dc, witness, kWhite, 1);
                drawBrowserWatcher(dc, (witness.left + witness.right) / 2, witness.top + px(88), px(53), 97 + browserPage_);
            }
            paintBrowserRemains(dc, page);
            RestoreDC(dc, viewport);
            paintBrowserLink(dc, layout);
            const int railX = layout.content.right - px(7);
            const int railH = link.top - layout.content.top - px(30);
            drawLine(dc, railX, layout.content.top + px(8), railX, link.top - px(8));
            const int thumbY = layout.content.top + px(8) + MulDiv(browserTabs_[browserActive_].scroll, railH - px(24), px(320));
            fillColor(dc, RECT{railX - px(2), thumbY, railX + px(2), thumbY + px(24)}, kWhite);
        }

    }

    // --------------------------------------------------------------- members
    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HANDLE embeddedFont_ = nullptr;
    HANDLE blackletterFontResource_ = nullptr;
    anaise::Audio audio_;

    // Offscreen mode: no window, a fixed client size, and a virtual clock.
    bool headless_ = false;
    int headlessWidth_ = 0;
    int headlessHeight_ = 0;
    ULONGLONG headlessNowMs_ = 0;

    UINT dpi_ = 96;
    HFONT titleFont_ = nullptr;
    HFONT headingFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HFONT dedicationFont_ = nullptr;
    anaise::NewGods gods_;
    anaise::Snake snake_;
    anaise::Tentacle tentacle_;
    bool godsMouseGuard_ = false;
    UINT fontsDpi_ = 0;

    bool fullscreen_ = false;
    WINDOWPLACEMENT placement_{};
    LONG windowedStyle_ = 0;

    std::vector<BrowserTab> browserTabs_{BrowserTab{}, BrowserTab{{1}, 0, 0}};
    int browserActive_ = 0;
    int browserPanel_ = 0;
    int browserPanelScroll_ = 0;
    bool browserEditing_ = false;
    bool browserSelectAll_ = false;
    std::wstring browserAddress_, browserQuery_;
    std::vector<int> browserVisits_{0};
    std::array<bool, kBrowserPages> browserBookmarks_{};
    int browserPage_ = 0;
    int browserHover_ = -1;
    ULONGLONG browserLoadStart_ = 0;
    ULONGLONG browserReloadStart_ = 0;

    Screen screen_ = Screen::Splash;
    ULONGLONG splashStart_ = 0;
    int menuIndex_ = 0;
    MenuPart menuHot_ = MenuPart::None;
    int menuHotIndex_ = -1;
    bool startMenuVisible_ = false;
    bool backgroundHidden_ = false;
    std::array<AppWindow, kAppCount> appWindows_{};
    std::array<int, kAppCount> windowOrder_{{0, 1, 2, 3, 4}};
    std::array<POINT, kMenuItems> iconPositions_{};
    std::array<bool, kMenuItems> iconPlaced_{};
    int pressedIcon_ = -1;
    bool iconDragged_ = false;
    RECT iconDragBounds_{};
    int draggingApp_ = -1;
    POINT dragStart_{};
    RECT dragBounds_{};

    anaise::RingBackground background_;

    // Cached surface and light fields for the pilgrim's dithered glow.
    HDC walkerDc_ = nullptr;
    HBITMAP walkerBitmap_ = nullptr;
    HGDIOBJ walkerOldBitmap_ = nullptr;
    std::uint32_t* walkerPixels_ = nullptr;
    int walkerWidth_ = 0;
    int walkerHeight_ = 0;
    std::vector<float> walkerLight_;
    std::vector<float> walkerTrail_;
    std::vector<float> walkerGlow_;
    std::vector<float> walkerScratch_;

    // Offscreen room, so the lens can bend it before it reaches the screen.
    HDC roomDc_ = nullptr;
    HBITMAP roomBitmap_ = nullptr;
    HGDIOBJ roomOldBitmap_ = nullptr;
    std::uint32_t* roomPixels_ = nullptr;
    int roomWidth_ = 0;
    int roomHeight_ = 0;
    std::vector<std::uint32_t> roomSource_;

    // The crash tears a full-frame copy of the library.
    HDC breakDc_ = nullptr;
    HBITMAP breakBitmap_ = nullptr;
    HGDIOBJ breakOldBitmap_ = nullptr;
    std::uint32_t* breakPixels_ = nullptr;
    int breakWidth_ = 0;
    int breakHeight_ = 0;

    // The corridor renders walls, floor and ghosts into this surface.
    HDC worldDc_ = nullptr;
    HBITMAP worldBitmap_ = nullptr;
    HGDIOBJ worldOldBitmap_ = nullptr;
    std::uint32_t* worldPixels_ = nullptr;
    int worldWidth_ = 0;
    int worldHeight_ = 0;
    std::vector<double> corridorDepth_;

    Walker walker_;
    ULONGLONG gameTickLast_ = 0;
    int lastDir_ = -1;

    // The run through the four rooms.
    int roomId_ = 0;
    bool key_ = false;
    bool keyTaken_ = false;
    std::array<bool, kLeverCount> levers_{};
    bool leverSolved_ = false;
    int plateStep_ = 0;
    bool plateSolved_ = false;
    ULONGLONG plateFlash_ = 0;
    std::wstring notice_;
    ULONGLONG noticeStart_ = 0;

    // The break: crash, search, corridor.
    GamePhase phase_ = GamePhase::Play;
    ULONGLONG phaseStart_ = 0;
    double camX_ = 1.5;
    double camY_ = 1.5;
    double camAngle_ = 0.0;
    anaise::SchoolHorror schoolHorror_;
    std::array<int, 4> schoolHearts_{};
    std::array<SchoolExpression, 4> schoolExpressions_{};
    std::array<ULONGLONG, 4> schoolReactionUntil_{};
    std::array<std::vector<std::uint32_t>, 4> schoolFacePixels_{};
    std::array<int, 4> schoolRounds_{};
    int schoolTalking_ = -1;
    int schoolDate_ = -1;
    int schoolRizz_ = 0;
    std::wstring schoolNotice_;
    ULONGLONG schoolNoticeStart_ = 0;
    ULONGLONG corridorTickLast_ = 0;

    std::array<std::array<bool, kPaintCols>, kPaintRows> canvas_{};
    std::vector<std::array<std::array<bool, kPaintCols>, kPaintRows>> undo_;
    std::vector<std::array<std::array<bool, kPaintCols>, kPaintRows>> redo_;
    int paintBrush_ = 1;
    bool painting_ = false;
    bool erasing_ = false;
    bool lastPaintValid_ = false;
    int lastPaintCol_ = 0;
    int lastPaintRow_ = 0;
    bool hoverValid_ = false;
    int hoverCol_ = 0;
    int hoverRow_ = 0;
    bool mouseTracking_ = false;
};

int runHeadless(HINSTANCE instance, const HeadlessOptions& options) {
    App app;
    if (!app.createHeadless(instance, options.width, options.height,
                            options.dpi)) {
        return 1;
    }
    if (options.checkWindows) return app.checkWindowInteractions() ? 0 : 1;
    if (options.checkBrowser) return app.checkBrowserInteractions() ? 0 : 1;
    if (options.checkGame) return app.checkGameInteractions() ? 0 : 1;
    if (options.checkSnake) return anaise::Snake::selfTest() && app.checkSnakeInteractions() ? 0 : 1;
    if (options.checkTentacle) return anaise::Tentacle::selfTest() && app.checkTentacleInteractions() ? 0 : 1;
    if (options.checkGods) return anaise::NewGods::selfTest() && app.checkGodsInteractions() ? 0 : 1;
    if (!ensureDirectory(options.outDir)) return 1;

    static const Screen kScreens[] = {Screen::Splash, Screen::Menu,
                                      Screen::Library, Screen::Paint,
                                      Screen::Browser, Screen::Crash,
                                      Screen::Search, Screen::Corridor,
                                      Screen::GodsRitual, Screen::GodsArena, Screen::GodsTower, Screen::Snake,
                                      Screen::Tentacle};
    const int count = options.allScreens
                          ? static_cast<int>(std::size(kScreens))
                          : 1;
    for (int i = 0; i < count; ++i) {
        const Screen screen = options.allScreens ? kScreens[i] : options.screen;
        std::wstring path = options.outDir;
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
            path.push_back(L'\\');
        }
        path += screenName(screen);
        path += L".png";
        if (!app.renderHeadlessPng(screen, options.timeMs, path,
                                   options.walkDir, options.walkProgress,
                                   options.browserPage, options.room, options.browserScroll, options.browserPanel,
                                   options.schoolView, options.schoolGirl, options.schoolExpression, options.godsView, options.snakeView)) {
            return 1;
        }
    }
    return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    enableDpiAwareness();

    HeadlessOptions options;
    if (!parseHeadlessOptions(commandLine, options)) return 2;
    if (options.checkAudio) {
        return anaise::Audio::resourcesPresent(instance) ? 0 : 3;
    }
    if (options.headless) return runHeadless(instance, options);

    App app;
    if (!app.create(instance)) return 1;

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
