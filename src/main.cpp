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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "angel_art.h"
#include "audio.h"
#include "occult_art.h"
#include "ring_pattern.h"

namespace {

constexpr int kLogoGrid = 10;

// Title typeface: Squealer by Raymond Larabie (Typodermic Fonts), released
// under CC0 1.0. See assets/fonts/squealer-LICENSE.txt.
constexpr int kTitleFontResource = 102;
const wchar_t* const kTitleFace = L"Squealer";

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
constexpr int kRoomCols = 13;
constexpr int kRoomRows = 9;
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

// '#' wall, 'B' bookcase, '.' floor.
const wchar_t* const kRoomMap[kRoomRows] = {
    L"#############",
    L"#...........#",
    L"#.B.......B.#",
    L"#...........#",
    L"#...........#",
    L"#...........#",
    L"#.B.......B.#",
    L"#...........#",
    L"#############",
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

// Loads the CC0 title typeface from the embedded resource, so the executable
// carries the font and needs nothing installed on the machine.
HANDLE loadEmbeddedFont(HINSTANCE instance) {
    HRSRC resource = FindResourceW(
        instance, MAKEINTRESOURCEW(kTitleFontResource), RT_RCDATA);
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

enum class Screen { Splash, Menu, Library, Paint, Browser };

const wchar_t* screenName(Screen screen) {
    switch (screen) {
        case Screen::Splash: return L"splash";
        case Screen::Menu: return L"menu";
        case Screen::Library: return L"library";
        case Screen::Paint: return L"paint";
        case Screen::Browser: return L"browser";
    }
    return L"screen";
}

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
    // Which Veil page a browser capture opens (1-based on the command line).
    int browserPage = 0;
    // Headless resource check: parse the embedded tracks without a device.
    bool checkAudio = false;
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
            else if (!options.allScreens) return false;
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
        if (embeddedFont_) {
            RemoveFontMemResourceEx(embeddedFont_);
            embeddedFont_ = nullptr;
        }
    }

    bool create(HINSTANCE instance) {
        instance_ = instance;
        embeddedFont_ = loadEmbeddedFont(instance);

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
        SetFocus(hwnd_);

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
        embeddedFont_ = loadEmbeddedFont(instance);
        return true;
    }

    bool renderHeadlessPng(Screen screen, ULONGLONG timeMs,
                           const std::wstring& path, int walkDir = -1,
                           double walkProgress = 0.5, int browserPage = 0) {
        if (!headless_) return false;
        screen_ = screen;
        headlessNowMs_ = timeMs;
        splashStart_ = 0;
        browserPage_ = std::clamp(browserPage, 0, kBrowserPages - 1);
        browserHover_ = -1;
        browserLoadStart_ = 0;
        browserReloadStart_ = 0;

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

private:
    static constexpr int kMenuItems = 4;
    static constexpr int kBrowserPages = kVeilPageCount;
    static constexpr int kBrowserHitBack = kBrowserPages;
    static constexpr int kBrowserHitForward = kBrowserPages + 1;
    static constexpr int kBrowserHitReload = kBrowserPages + 2;
    static constexpr int kBrowserHitLink = kBrowserPages + 3;
    const wchar_t* const kMenuLabels[kMenuItems] = {
        L"BETHELAMENT",
        L"PAINTER",
        L"VEIL",
        L"QUIT",
    };

    // The menu is a small monochrome operating system: every option is an app
    // icon on the desktop, with a finder window and a taskbar around it.
    const wchar_t* const kMenuTitles[kMenuItems] = {
        L"BETHELAMENT.EXE",
        L"PAINTER.EXE",
        L"VEIL.EXE",
        L"SHUTDOWN.EXE",
    };
    const wchar_t* const kMenuKinds[kMenuItems] = {
        L"APPLICATION / ROOM",
        L"APPLICATION / CANVAS",
        L"APPLICATION / NETWORK",
        L"SYSTEM / SHUTDOWN",
    };
    const wchar_t* const kMenuBlurbs[kMenuItems] = {
        L"A QUIET ROOM OF SHELVES.\nWALK ITS FLOOR IN THE DARK.",
        L"A 32 X 32 GRID OF LIGHT.\nDRAW WITH THE LEFT BUTTON.",
        L"THE NETWORK IS A MOUTH.\nIT OPENS FROM THE INSIDE.",
        L"CLOSE THE MACHINE.\nNOTHING IS SAVED.",
    };
    const wchar_t* const kMenuHints[kMenuItems] = {
        L"WASD / ARROWS   ESC TO LEAVE",
        L"C CLEAR  1-3/WHEEL  CTRL+Z  CTRL+Y",
        L"ARROWS / WHEEL   TAB   1-9 PAGES",
        L"ESC TO CANCEL",
    };
    const wchar_t* const kMenuFacts[kMenuItems] = {
        L"SIZE   13 X 9 TILES\nLIGHT  QUIET\nSTATE  STILL",
        L"SIZE   32 X 32 CELLS\nLIGHT  WHITE\nSTATE  READY",
        L"SIZE   15 PAGES\nLIGHT  NONE\nSTATE  WATCHING",
        L"SIZE   0 X 0\nLIGHT  OFF\nSTATE  FINAL",
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
            L".......##.......",
            L"......####......",
            L".....######.....",
            L"....########....",
            L"....##....##....",
            L"....##.##.##....",
            L"....##.##.##....",
            L"....##.##.##....",
            L"....##.##.##....",
            L"....##.##.##....",
            L"....##....##....",
            L"...##########...",
            L"...##########...",
            L"...##########...",
            L"................",
        };
        static const wchar_t* const kPainter[16] = {
            L"................",
            L"............####",
            L"...........####.",
            L"..........####..",
            L".........####...",
            L"........####....",
            L".......####.....",
            L"......####......",
            L".....####.......",
            L"....####........",
            L"...####.........",
            L"..####..........",
            L".####...........",
            L"###.............",
            L"#...............",
            L"................",
        };
        static const wchar_t* const kVeil[16] = {
            L"................",
            L".##############.",
            L".#............#.",
            L".#.##.##.##.##.#",
            L".#....####....#.",
            L".#..########..#.",
            L".#.##########.#.",
            L".##.###..###.##.",
            L".#.##########.#.",
            L".#..########..#.",
            L".#....####....#.",
            L".#............#.",
            L".##############.",
            L"................",
            L"................",
            L"................",
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
            default: return kShutdown;
        }
    }

    enum class MenuPart {
        None,
        Icon,
        Open,
        Start,
        StartItem,
        TaskChip,
        WindowClose,
        WindowMax,
        WindowMin,
        SectionPrograms,
        SectionRooms,
        SectionLight,
        Signal,
    };

    struct MenuHit {
        MenuPart part = MenuPart::None;
        int index = -1;
    };

    struct MenuLayout {
        RECT monitor;
        RECT screen;
        RECT menuBar;
        RECT desktop;
        RECT taskBar;
        RECT panel;
        RECT iconTile[kMenuItems];
        RECT iconLabel[kMenuItems];
        RECT iconHit[kMenuItems];
        RECT window;
        RECT windowTitle;
        RECT windowBody;
        RECT previewTile;
        RECT openButton;
        // The shell's working buttons. They are stored here so painting and
        // hit testing use exactly the same rectangles.
        RECT startButton;
        RECT taskChip[kMenuItems];
        RECT windowMin;
        RECT windowMax;
        RECT windowClose;
        RECT sectionPrograms;
        RECT sectionRooms;
        RECT sectionLight;
        RECT signal;
        RECT startMenu;
        RECT startItem[kMenuItems];
    };

    struct GridLayout {
        int left = 0;
        int top = 0;
        int cell = 0;
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
        RECT content;
        RECT status;
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
                InvalidateRect(hwnd_, nullptr, FALSE);
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
                InvalidateRect(hwnd_, nullptr, FALSE);
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
                    if (screen_ == Screen::Paint) {
                        cursor = IDC_CROSS;
                    } else if (screen_ == Screen::Menu &&
                               menuHot_ != MenuPart::None) {
                        cursor = IDC_HAND;
                    } else if (screen_ == Screen::Browser &&
                               browserHover_ >= 0) {
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
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                if (menuHot_ != MenuPart::None) {
                    menuHot_ = MenuPart::None;
                    menuHotIndex_ = -1;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;

            case WM_MOUSEWHEEL: {
                const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta == 0) return 0;
                if (screen_ == Screen::Paint && !painting_) {
                    paintBrush_ = std::clamp(
                        paintBrush_ + (delta > 0 ? 1 : -1), 1, 3);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
                if (screen_ == Screen::Browser) {
                    browserGo(browserPage_ + (delta > 0 ? -1 : 1));
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
                painting_ = false;
                lastPaintValid_ = false;
                return 0;

            case WM_KEYDOWN:
                if (wParam == VK_F11 && !(lParam & (1 << 30))) {
                    setFullscreen(!fullscreen_);
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    if (fullscreen_) {
                        setFullscreen(false);
                        return 0;
                    }
                    if (screen_ == Screen::Splash ||
                        screen_ == Screen::Library || screen_ == Screen::Paint ||
                        screen_ == Screen::Browser) {
                        enterMenu();
                        return 0;
                    }
                    if (screen_ == Screen::Menu) {
                        if (startMenuVisible_) {
                            startMenuVisible_ = false;
                            InvalidateRect(hwnd_, nullptr, FALSE);
                            return 0;
                        }
                        DestroyWindow(hwnd_);
                        return 0;
                    }
                }
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
                audio_.close();
                if (embeddedFont_) {
                    RemoveFontMemResourceEx(embeddedFont_);
                    embeddedFont_ = nullptr;
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
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void tickFrame() {
        // The splash carries its own timer and no ring pattern; this one only
        // keeps the pattern turning behind the working screens.
        if (screen_ == Screen::Splash || !hwnd_) return;
        if (IsIconic(hwnd_)) return;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void enterMenu() {
        KillTimer(hwnd_, kSplashTimer);
        KillTimer(hwnd_, kGameTimer);
        stopWalker();
        if (painting_) {
            painting_ = false;
            lastPaintValid_ = false;
            if (GetCapture() == hwnd_) ReleaseCapture();
        }
        screen_ = Screen::Menu;
        if (hwnd_) SetWindowTextW(hwnd_, L"Anaise");
        menuHot_ = MenuPart::None;
        menuHotIndex_ = -1;
        startMenuVisible_ = false;
        audio_.play(anaise::Music::Menu);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void openScreen(Screen screen) {
        if (painting_) {
            painting_ = false;
            lastPaintValid_ = false;
            if (GetCapture() == hwnd_) ReleaseCapture();
        }
        if (screen == Screen::Library) {
            lastDir_ = -1;
            gameTickLast_ = GetTickCount64();
            SetTimer(hwnd_, kGameTimer, kGameTickMs, nullptr);
        } else {
            KillTimer(hwnd_, kGameTimer);
        }
        if (screen == Screen::Browser) {
            browserHover_ = -1;
            browserLoadStart_ = nowMs();
        }
        screen_ = screen;
        if (hwnd_) {
            switch (screen) {
                case Screen::Library:
                    SetWindowTextW(hwnd_, L"Anaise - Bethelament");
                    break;
                case Screen::Paint:
                    SetWindowTextW(hwnd_, L"Anaise - Painter");
                    break;
                case Screen::Browser:
                    SetWindowTextW(hwnd_, L"Anaise - Veil");
                    break;
                default:
                    SetWindowTextW(hwnd_, L"Anaise");
                    break;
            }
        }
        startMenuVisible_ = false;
        menuHot_ = MenuPart::None;
        menuHotIndex_ = -1;
        audio_.play(screen == Screen::Library || screen == Screen::Paint
                        ? anaise::Music::Room
                        : anaise::Music::Menu);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void activateMenuItem() {
        switch (menuIndex_) {
            case 0:
                openScreen(Screen::Library);
                break;
            case 1:
                openScreen(Screen::Paint);
                break;
            case 2:
                openScreen(Screen::Browser);
                break;
            default:
                DestroyWindow(hwnd_);
                break;
        }
    }

    // ---------------------------------------------------------------- browser
    void browserGo(int page) {
        const int count = kBrowserPages;
        page = ((page % count) + count) % count;
        browserPage_ = page;
        browserHover_ = -1;
        browserLoadStart_ = nowMs();
        InvalidateRect(hwnd_, nullptr, FALSE);
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
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    bool browserLoading() const {
        if (browserLoadStart_ == 0) return false;
        return nowMs() - browserLoadStart_ < 720;
    }

    void handleKey(int key) {
        // The signal bars are a small target; M mutes from any screen.
        if (key == 'M') {
            audio_.toggleMuted();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (screen_ == Screen::Splash) {
            enterMenu();
            return;
        }
        if (screen_ == Screen::Menu) {
            if (key == VK_UP || key == 'W') {
                menuIndex_ = (menuIndex_ + kMenuItems - 1) % kMenuItems;
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (key == VK_DOWN || key == 'S') {
                menuIndex_ = (menuIndex_ + 1) % kMenuItems;
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (key == VK_RETURN || key == VK_SPACE) {
                activateMenuItem();
            }
            return;
        }
        if (screen_ == Screen::Browser) {
            if (key == VK_LEFT || key == 'A' || key == VK_BACK ||
                key == '[') {
                browserGo(browserPage_ - 1);
            } else if (key == VK_RIGHT || key == 'D' || key == VK_RETURN ||
                       key == VK_SPACE || key == VK_TAB || key == ']') {
                browserFollowLink();
            } else if (key >= '1' && key <= '9') {
                browserGo(key - '1');
            } else if (key == '0') {
                browserGo(9);
            } else if (key == VK_HOME) {
                browserGo(0);
            } else if (key == VK_END) {
                browserGo(kBrowserPages - 1);
            } else if (key == 'R') {
                browserReload();
            }
            return;
        }
        if (screen_ == Screen::Paint) {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (key == 'C') {
                clearCanvas();
            } else if (key == '1' || key == '2' || key == '3') {
                paintBrush_ = key - '0';
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (key == 'Z' && ctrl && shift) {
                redoCanvas();
            } else if (key == 'Z' && ctrl) {
                undoCanvas();
            } else if (key == 'Y' && ctrl) {
                redoCanvas();
            }
        } else if (screen_ == Screen::Library) {
            const int dir = directionForKey(key);
            if (dir >= 0) lastDir_ = dir;
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
    static bool roomWalkable(int col, int row) {
        if (col < 0 || col >= kRoomCols || row < 0 || row >= kRoomRows) {
            return false;
        }
        const wchar_t tile = kRoomMap[row][col];
        return tile != L'#' && tile != L'B';
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
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void tickGame() {
        if (screen_ != Screen::Library) return;
        const ULONGLONG now = GetTickCount64();
        double elapsed = static_cast<double>(now - gameTickLast_);
        gameTickLast_ = now;
        if (elapsed > 120.0) elapsed = 120.0;
        if (updateWalker(elapsed)) InvalidateRect(hwnd_, nullptr, FALSE);
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
        if (!mouseTracking_) {
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd_;
            TrackMouseEvent(&track);
            mouseTracking_ = true;
        }

        if (screen_ == Screen::Menu) {
            const MenuHit hit = menuHitAt(x, y);
            bool changed = hit.part != menuHot_ || hit.index != menuHotIndex_;
            menuHot_ = hit.part;
            menuHotIndex_ = hit.index;
            if (hit.part == MenuPart::Icon && hit.index != menuIndex_) {
                menuIndex_ = hit.index;
                changed = true;
            }
            if (changed) InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        if (screen_ == Screen::Browser) {
            const int hit = browserHitAt(x, y);
            if (hit != browserHover_) {
                browserHover_ = hit;
                InvalidateRect(hwnd_, nullptr, FALSE);
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
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
        } else if (!hoverValid_ || col != hoverCol_ || row != hoverRow_) {
            hoverValid_ = true;
            hoverCol_ = col;
            hoverRow_ = row;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void onMouseDown(bool leftButton, int x, int y) {
        SetFocus(hwnd_);

        if (screen_ == Screen::Splash) {
            enterMenu();
            return;
        }
        if (screen_ == Screen::Menu) {
            if (!leftButton) return;
            const MenuHit hit = menuHitAt(x, y);
            if (startMenuVisible_ && hit.part != MenuPart::Start &&
                hit.part != MenuPart::StartItem) {
                startMenuVisible_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
            switch (hit.part) {
                case MenuPart::Icon:
                    menuIndex_ = hit.index;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    activateMenuItem();
                    break;
                case MenuPart::Open:
                    activateMenuItem();
                    break;
                case MenuPart::Start:
                    startMenuVisible_ = !startMenuVisible_;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    break;
                case MenuPart::StartItem:
                    menuIndex_ = hit.index;
                    startMenuVisible_ = false;
                    activateMenuItem();
                    break;
                case MenuPart::TaskChip:
                    menuIndex_ = hit.index;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    activateMenuItem();
                    break;
                case MenuPart::WindowClose:
                    DestroyWindow(hwnd_);
                    break;
                case MenuPart::WindowMax:
                    ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE
                                                      : SW_MAXIMIZE);
                    break;
                case MenuPart::WindowMin:
                    ShowWindow(hwnd_, SW_MINIMIZE);
                    break;
                case MenuPart::Signal:
                    audio_.toggleMuted();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    break;
                case MenuPart::SectionPrograms:
                    activateMenuItem();
                    break;
                case MenuPart::SectionRooms:
                    menuIndex_ = 0;
                    openScreen(Screen::Library);
                    break;
                case MenuPart::SectionLight:
                    backgroundHidden_ = !backgroundHidden_;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    break;
                default:
                    break;
            }
            return;
        }
        if (screen_ == Screen::Browser) {
            if (!leftButton) return;
            const int hit = browserHitAt(x, y);
            if (hit >= 0 && hit < kBrowserPages) {
                browserGo(hit);
            } else if (hit == kBrowserHitBack) {
                browserGo(browserPage_ - 1);
            } else if (hit == kBrowserHitForward ||
                       hit == kBrowserHitLink) {
                browserFollowLink();
            } else if (hit == kBrowserHitReload) {
                browserReload();
            }
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
        SetCapture(hwnd_);
        paintTo(x, y);
    }

    void onMouseUp() {
        if (!painting_) return;
        painting_ = false;
        lastPaintValid_ = false;
        if (GetCapture() == hwnd_) ReleaseCapture();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    MenuHit menuHitAt(int x, int y) const {
        RECT client{};
        GetClientRect(hwnd_, &client);
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
        for (int i = 0; i < kMenuItems; ++i) {
            if (inside(layout.iconHit[i])) {
                return MenuHit{MenuPart::Icon, i};
            }
        }
        // The window's OPEN plate acts on the app that is already selected.
        if (inside(layout.openButton)) return MenuHit{MenuPart::Open, menuIndex_};
        if (inside(layout.windowClose)) {
            return MenuHit{MenuPart::WindowClose, -1};
        }
        if (inside(layout.windowMax)) {
            return MenuHit{MenuPart::WindowMax, -1};
        }
        if (inside(layout.windowMin)) {
            return MenuHit{MenuPart::WindowMin, -1};
        }
        if (inside(layout.startButton)) return MenuHit{MenuPart::Start, -1};
        for (int i = 0; i < kMenuItems; ++i) {
            if (inside(layout.taskChip[i])) {
                return MenuHit{MenuPart::TaskChip, i};
            }
        }
        if (inside(layout.signal)) return MenuHit{MenuPart::Signal, -1};
        if (inside(layout.sectionPrograms)) {
            return MenuHit{MenuPart::SectionPrograms, -1};
        }
        if (inside(layout.sectionRooms)) {
            return MenuHit{MenuPart::SectionRooms, -1};
        }
        if (inside(layout.sectionLight)) {
            return MenuHit{MenuPart::SectionLight, -1};
        }
        return MenuHit{};
    }

    MenuLayout menuLayout(int width, int height) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };

        MenuLayout layout;
        const int margin = px(8);
        const int bezel = px(22);
        layout.monitor =
            RECT{margin, margin, width - margin, height - margin};
        layout.screen = RECT{margin + bezel, margin + bezel,
                             width - margin - bezel, height - margin - bezel};
        if (layout.screen.right <= layout.screen.left + px(40) ||
            layout.screen.bottom <= layout.screen.top + px(40)) {
            layout.screen = RECT{0, 0, width, height};
            layout.monitor = layout.screen;
        }

        const int menuHeight = px(26);
        const int taskHeight = px(30);
        layout.menuBar = RECT{layout.screen.left, layout.screen.top,
                              layout.screen.right,
                              layout.screen.top + menuHeight};
        layout.taskBar = RECT{layout.screen.left,
                              layout.screen.bottom - taskHeight,
                              layout.screen.right, layout.screen.bottom};
        layout.desktop = RECT{layout.screen.left, layout.menuBar.bottom,
                              layout.screen.right, layout.taskBar.top};

        int tile = px(56);
        const int labelHeight = px(16);
        const int iconGap = px(8);
        const int panelWidth = tile + px(44);
        const int headerHeight = px(24);
        const int panelPad = px(10);
        const int desktopHeight = layout.desktop.bottom - layout.desktop.top;
        const int maxPanelHeight = std::max(px(180), desktopHeight - px(32));
        const int wantedHeight = headerHeight + kMenuItems * (tile + px(2) +
                                   labelHeight + iconGap) - iconGap + panelPad;
        if (wantedHeight > maxPanelHeight) {
            const int fixed = headerHeight + panelPad - iconGap +
                              kMenuItems * (px(2) + labelHeight + iconGap);
            tile = std::max(px(28), (maxPanelHeight - fixed) / kMenuItems);
        }
        const int panelHeight = headerHeight + kMenuItems * (tile + px(2) +
                                  labelHeight + iconGap) - iconGap + panelPad;
        layout.panel = RECT{layout.desktop.left + px(16),
                            layout.desktop.top + px(16),
                            layout.desktop.left + px(16) + panelWidth,
                            layout.desktop.top + px(16) + panelHeight};

        for (int i = 0; i < kMenuItems; ++i) {
            const int top = layout.panel.top + headerHeight +
                            i * (tile + px(2) + labelHeight + iconGap);
            const int left = layout.panel.left + (panelWidth - tile) / 2;
            layout.iconTile[i] = RECT{left, top, left + tile, top + tile};
            layout.iconLabel[i] =
                RECT{layout.panel.left + px(4), top + tile + px(2),
                     layout.panel.right - px(4), top + tile + px(2) +
                                                    labelHeight};
            layout.iconHit[i] =
                RECT{layout.panel.left + px(2), top - px(3),
                     layout.panel.right - px(2),
                     layout.iconLabel[i].bottom + px(3)};
        }

        const int availableLeft = layout.panel.right + px(26);
        const int availableRight = layout.desktop.right - px(18);
        const int availableWidth = std::max(px(240), availableRight - availableLeft);
        const int windowWidth = std::min(px(620), availableWidth);
        int windowHeight = std::min(px(330), desktopHeight - px(36));
        windowHeight = std::max(px(220), windowHeight);
        const int windowLeft =
            availableLeft + (availableWidth - windowWidth) / 2;
        const int windowTop =
            layout.desktop.top + (desktopHeight - windowHeight) / 2;
        layout.window = RECT{windowLeft, windowTop,
                             windowLeft + windowWidth,
                             windowTop + windowHeight};
        layout.windowTitle =
            RECT{layout.window.left, layout.window.top, layout.window.right,
                 layout.window.top + px(24)};
        layout.windowBody =
            RECT{layout.window.left, layout.windowTitle.bottom,
                 layout.window.right, layout.window.bottom};

        int preview = std::min(px(96), windowHeight - px(150));
        preview = std::max(px(56), preview);
        layout.previewTile =
            RECT{layout.window.left + px(20),
                 layout.windowBody.top + px(20),
                 layout.window.left + px(20) + preview,
                 layout.windowBody.top + px(20) + preview};
        layout.openButton =
            RECT{layout.window.right - px(20) - px(92),
                 layout.window.bottom - px(20) - px(26),
                 layout.window.right - px(20),
                 layout.window.bottom - px(20)};

        // Title-bar controls, right to left: close, zoom, minimise.
        const RECT& title = layout.windowTitle;
        const int box = px(12);
        const int boxTop = title.top + (title.bottom - title.top - box) / 2;
        RECT* const windowBoxes[3] = {&layout.windowClose, &layout.windowMax,
                                      &layout.windowMin};
        for (int i = 0; i < 3; ++i) {
            const int left = title.right - px(8) - (i + 1) * px(16);
            *windowBoxes[i] = RECT{left, boxTop, left + box, boxTop + box};
        }

        // The taskbar: start plate, then one chip per app, then the clock and
        // the signal bars at the far right.
        const RECT& bar = layout.taskBar;
        layout.startButton =
            RECT{bar.left + px(6), bar.top + px(4),
                 bar.left + px(6) + px(86), bar.bottom - px(4)};
        int chipLeft = layout.startButton.right + px(10);
        for (int i = 0; i < kMenuItems; ++i) {
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

        // Menubar sections, as three separate clickable plates.
        const wchar_t* const sections[3] = {L"PROGRAMS", L"ROOMS", L"LIGHT"};
        RECT* const sectionRects[3] = {&layout.sectionPrograms,
                                       &layout.sectionRooms,
                                       &layout.sectionLight};
        int sectionWidth = 0;
        for (const wchar_t* text : sections) {
            sectionWidth += static_cast<int>(std::wcslen(text)) * px(7);
        }
        sectionWidth += 2 * px(18);
        int sectionLeft = (layout.menuBar.left + layout.menuBar.right) / 2 -
                          sectionWidth / 2;
        for (int i = 0; i < 3; ++i) {
            const int width = static_cast<int>(std::wcslen(sections[i])) * px(7);
            *sectionRects[i] =
                RECT{sectionLeft, layout.menuBar.top, sectionLeft + width,
                     layout.menuBar.bottom};
            sectionLeft += width + px(18);
        }

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
        const int margin = px(34);
        const int headerBottom =
            margin + std::max(2, px(5)) * kLogoGrid + px(20);
        const int top = headerBottom + px(24);
        const int bottom = std::max(top + px(230), height - px(58));
        layout.window = RECT{margin, top, width - margin, bottom};
        layout.title =
            RECT{layout.window.left, layout.window.top, layout.window.right,
                 layout.window.top + px(24)};

        static constexpr int kTabWidths[kBrowserPages] = {
            54, 54, 58, 58, 58, 58, 58, 58, 50,
            66, 62, 68, 68, 70, 46,
        };
        const int tabGap = px(3);
        const int tabHeight = px(23);
        const int tabLeftEdge = layout.window.left + px(8);
        int totalWidth = tabGap * (kBrowserPages - 1);
        for (int width : kTabWidths) totalWidth += px(width);
        const int availableWidth = layout.window.right - px(8) - tabLeftEdge;
        const int rows = totalWidth <= availableWidth ? 1 : 2;
        const int perRow = (kBrowserPages + rows - 1) / rows;
        const int tabTop = layout.title.bottom;
        int rowLeft = tabLeftEdge;
        for (int i = 0; i < kBrowserPages; ++i) {
            const int column = i % perRow;
            if (column == 0) rowLeft = tabLeftEdge;
            const int tabWidth = px(kTabWidths[i]);
            const int top = tabTop + (i / perRow) * tabHeight;
            layout.tabs[i] =
                RECT{rowLeft, top + px(3), rowLeft + tabWidth,
                     top + tabHeight};
            rowLeft += tabWidth + tabGap;
        }
        layout.tabsBottom = tabTop + rows * tabHeight;

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
                 layout.window.right - px(126), addressTop + px(20)};

        layout.status = RECT{layout.window.left,
                             layout.window.bottom - px(22),
                             layout.window.right, layout.window.bottom};
        layout.content = RECT{layout.window.left + 1, toolbarBottom,
                              layout.window.right - 1, layout.status.top};
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
        RECT client{};
        GetClientRect(hwnd_, &client);
        const BrowserLayout layout =
            browserLayout(client.right, client.bottom);
        const POINT point{x, y};
        auto inside = [&](const RECT& rect) {
            return PtInRect(&rect, point) != 0;
        };
        for (int i = 0; i < kBrowserPages; ++i) {
            if (inside(layout.tabs[i])) return i;
        }
        if (inside(layout.back)) return kBrowserHitBack;
        if (inside(layout.forward)) return kBrowserHitForward;
        if (inside(layout.reload)) return kBrowserHitReload;
        if (inside(browserLinkRect(layout))) return kBrowserHitLink;
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
        RECT client{};
        GetClientRect(hwnd_, &client);
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
        InvalidateRect(hwnd_, nullptr, FALSE);
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
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void redoCanvas() {
        if (redo_.empty()) return;
        if (undo_.size() >= 64) undo_.erase(undo_.begin());
        undo_.push_back(canvas_);
        canvas_ = redo_.back();
        redo_.pop_back();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void clearCanvas() {
        pushUndo();
        for (auto& row : canvas_) row.fill(false);
        InvalidateRect(hwnd_, nullptr, FALSE);
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
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // --------------------------------------------------------------- drawing
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
        titleFont_ = nullptr;
        headingFont_ = nullptr;
        bodyFont_ = nullptr;
        smallFont_ = nullptr;
        fontsDpi_ = 0;
    }

    void ensureFonts() {
        if (fontsDpi_ == dpi_ && titleFont_ && headingFont_ && bodyFont_ &&
            smallFont_) {
            return;
        }
        releaseFonts();

        auto makeFont = [this](int points, const wchar_t* face) {
            LOGFONTW lf{};
            lf.lfWeight = FW_NORMAL;
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfQuality = ANTIALIASED_QUALITY;
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
        info.bmiHeader.biWidth = 8;
        info.bmiHeader.biHeight = -8;  // top-down rows
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
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                pixels[row * 8 + col] =
                    kBayer[row][col] < level ? 0x00FFFFFFu : 0x00000000u;
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
        if (selected) {
            fillColor(dc, tile, kWhite);
        } else {
            fillDither(dc, tile, 9);
            strokeRect(dc, tile, kWhite, 1);
        }
        const int tileWidth = tile.right - tile.left;
        const int cell = std::max(1, (tileWidth - MulDiv(8, dpi_, 96)) / 16);
        const int glyph = cell * 16;
        const int left = tile.left + (tileWidth - glyph) / 2;
        const int top = tile.top + (tileWidth - glyph) / 2;
        drawGlyph(dc, appIconRows(index), left, top, cell,
                  selected ? kBlack : kWhite);
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

    void paintMenuBar(HDC dc, const MenuLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& bar = layout.menuBar;
        fillColor(dc, bar, kBlack);
        drawLine(dc, bar.left, bar.bottom - 1, bar.right, bar.bottom - 1);

        const int logoCell = std::max(1, px(1));
        const int logoPixels = kLogoGrid * logoCell;
        const int textH = textHeight(dc, smallFont_);
        const int centerY = bar.top + (bar.bottom - bar.top - textH) / 2;
        drawLogo(dc, bar.left + px(8), centerY + textH / 2 - logoPixels / 2,
                 logoCell);
        const int brandX = bar.left + px(8) + logoPixels + px(8);
        drawTextAt(dc, smallFont_, L"ANAISE OS", brandX, centerY, kWhite);

        const wchar_t* const sections[3] = {L"PROGRAMS", L"ROOMS", L"LIGHT"};
        const RECT* const rects[3] = {&layout.sectionPrograms,
                                      &layout.sectionRooms,
                                      &layout.sectionLight};
        const MenuPart parts[3] = {MenuPart::SectionPrograms,
                                   MenuPart::SectionRooms,
                                   MenuPart::SectionLight};
        for (int i = 0; i < 3; ++i) {
            drawTextAt(dc, smallFont_, sections[i], rects[i]->left, centerY,
                       kWhite);
            if (hot(parts[i])) {
                drawLine(dc, rects[i]->left, rects[i]->bottom - px(4),
                         rects[i]->right, rects[i]->bottom - px(4));
            }
        }

        const std::wstring clock = clockText();
        const int clockWidth = textWidth(dc, smallFont_, clock);
        const int clockX = bar.right - px(10) - clockWidth;
        drawTextAt(dc, smallFont_, clock, clockX, centerY, kWhite);
        HBRUSH signalBrush = CreateSolidBrush(kWhite);
        const int signalLeft = clockX - px(30);
        for (int i = 0; i < 4; ++i) {
            const int barHeight = px(2 + i * 2);
            RECT signal{signalLeft + i * px(4), centerY + textH - barHeight,
                        signalLeft + i * px(4) + px(3), centerY + textH};
            FillRect(dc, &signal, signalBrush);
        }
        DeleteObject(signalBrush);
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

    void paintAppPanel(HDC dc, const MenuLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& panel = layout.panel;
        fillColor(dc, panel, kBlack);
        strokeRect(dc, panel, kWhite, 1);
        const int headerH = px(24);
        const int textH = textHeight(dc, smallFont_);
        drawTextCentered(dc, smallFont_, L"PROGRAMS",
                         (panel.left + panel.right) / 2,
                         panel.top + (headerH - textH) / 2, kWhite);
        drawLine(dc, panel.left + px(6), panel.top + headerH - 1,
                 panel.right - px(6), panel.top + headerH - 1);

        for (int i = 0; i < kMenuItems; ++i) {
            const bool selected = i == menuIndex_;
            drawMenuIcon(dc, i, layout.iconTile[i], selected);
            const RECT& label = layout.iconLabel[i];
            const int labelY =
                label.top + (label.bottom - label.top - textH) / 2;
            if (selected) {
                fillColor(dc, label, kWhite);
                drawTextCentered(dc, smallFont_, kMenuLabels[i],
                                 (label.left + label.right) / 2, labelY,
                                 kBlack);
            } else {
                drawTextCentered(dc, smallFont_, kMenuLabels[i],
                                 (label.left + label.right) / 2, labelY,
                                 kWhite);
            }
            if (selected) {
                RECT marquee{layout.iconHit[i].left - px(3),
                             layout.iconHit[i].top - px(3),
                             layout.iconHit[i].right + px(3),
                             layout.iconHit[i].bottom + px(3)};
                drawDashedRect(dc, marquee, px(3), px(2));
            }
        }
    }

    void paintAppWindow(HDC dc, const MenuLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const int index = menuIndex_;
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

        // Close, zoom and minimise boxes, right to left. They are live: the
        // close box quits, zoom toggles maximise and minimise does what it says.
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

        const RECT& body = layout.windowBody;
        fillDither(dc, layout.previewTile, 8);
        strokeRect(dc, layout.previewTile, kWhite, 1);
        const int previewWidth = layout.previewTile.right -
                                 layout.previewTile.left;
        const int previewCell = std::max(1, (previewWidth - px(16)) / 16);
        const int glyph = previewCell * 16;
        drawGlyph(dc, appIconRows(index),
                  layout.previewTile.left +
                      (previewWidth - glyph) / 2,
                  layout.previewTile.top + (previewWidth - glyph) / 2,
                  previewCell, kWhite);

        const std::wstring tag = L"APP 0" + std::to_wstring(index + 1);
        const int smallH = textHeight(dc, smallFont_);
        drawTextCentered(dc, smallFont_, tag,
                         (layout.previewTile.left +
                          layout.previewTile.right) / 2,
                         layout.previewTile.bottom + px(8), kWhite);

        const int textX = layout.previewTile.right + px(28);
        const int textRight = window.right - px(20);
        int y = body.top + px(20);
        drawTextAt(dc, smallFont_, kMenuKinds[index], textX, y, kWhite);
        y += smallH + px(10);
        drawLine(dc, textX, y, textRight, y);
        y += px(14);

        const int bodyTextH = textHeight(dc, bodyFont_);
        const wchar_t* blurb = kMenuBlurbs[index];
        std::wstring line;
        for (const wchar_t* p = blurb;; ++p) {
            if (*p == L'\n' || *p == L'\0') {
                drawTextAt(dc, bodyFont_, line, textX, y, kWhite);
                y += bodyTextH + px(4);
                line.clear();
                if (*p == L'\0') break;
            } else {
                line.push_back(*p);
            }
        }

        y += px(14);
        drawTextAt(dc, smallFont_, kMenuHints[index], textX, y, kWhite);

        // A small file inspector under the blurb. It also keeps the window
        // from going hollow when the desktop is tall.
        const int factsTop =
            std::max(y + px(32), static_cast<int>(body.top) + px(160));
        if (layout.openButton.top - px(12) >= factsTop + smallH) {
            drawLine(dc, textX, factsTop - px(8), textRight,
                     factsTop - px(8));
            int factsY = factsTop;
            const wchar_t* facts = kMenuFacts[index];
            std::wstring factLine;
            for (const wchar_t* p = facts;; ++p) {
                if (*p == L'\n' || *p == L'\0') {
                    if (factsY + smallH > layout.openButton.top - px(10)) break;
                    drawTextAt(dc, smallFont_, factLine, textX, factsY, kWhite);
                    factsY += smallH + px(6);
                    factLine.clear();
                    if (*p == L'\0') break;
                } else {
                    factLine.push_back(*p);
                }
            }
        }

        // Etched seal in the quiet corner above OPEN.
        const int sealRadius = std::clamp(
            (static_cast<int>(layout.openButton.top) -
             static_cast<int>(body.top)) / 6,
            px(18), px(46));
        const int sealX = textRight - sealRadius - px(8);
        const int sealY = layout.openButton.top - sealRadius - px(16);
        drawSigil(dc, sealX, sealY, sealRadius);

        const int statusY = layout.openButton.top +
                            (layout.openButton.bottom -
                             layout.openButton.top - smallH) / 2;
        drawTextAt(dc, smallFont_, L"ENTER TO OPEN", window.left + px(20),
                   statusY, kWhite);

        const bool hotOpen = hot(MenuPart::Open);
        if (hotOpen) {
            fillColor(dc, layout.openButton, kWhite);
            drawTextCentered(dc, smallFont_, L"OPEN",
                             (layout.openButton.left +
                              layout.openButton.right) / 2,
                             statusY, kBlack);
        } else {
            strokeRect(dc, layout.openButton, kWhite, 1);
            drawTextCentered(dc, smallFont_, L"OPEN",
                             (layout.openButton.left +
                              layout.openButton.right) / 2,
                             statusY, kWhite);
        }
    }

    void paintTaskBar(HDC dc, const MenuLayout& layout) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& bar = layout.taskBar;
        fillColor(dc, bar, kBlack);
        drawLine(dc, bar.left, bar.top, bar.right, bar.top);

        const int textH = textHeight(dc, smallFont_);
        const int centerY = bar.top + (bar.bottom - bar.top - textH) / 2;
        const std::wstring brand = L"ANAISE";
        const int logoCell = std::max(1, px(1));
        const int logoPixels = kLogoGrid * logoCell;
        const RECT& start = layout.startButton;
        fillColor(dc, start, kWhite);
        drawLogo(dc, start.left + px(5),
                 start.top + (start.bottom - start.top - logoPixels) / 2,
                 logoCell, kBlack);
        const int brandWidth = textWidth(dc, smallFont_, brand);
        drawTextAt(dc, smallFont_, brand,
                   start.right - px(7) - brandWidth, centerY, kBlack);
        if (hot(MenuPart::Start)) {
            drawDashedRect(dc, start, px(3), px(2), kBlack);
        }

        for (int i = 0; i < kMenuItems; ++i) {
            const std::wstring label = kMenuLabels[i];
            const RECT& chip = layout.taskChip[i];
            if (i == menuIndex_) {
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
        HBRUSH signalBrush = CreateSolidBrush(kWhite);
        const int bars = 4;
        const int gap = px(2);
        const int barWidth = px(3);
        const int barsWidth = bars * barWidth + (bars - 1) * gap;
        const int barsLeft = signal.right - barsWidth;
        for (int i = 0; i < bars; ++i) {
            const int barHeight = px(3 + i * 2);
            RECT barsRect{barsLeft + i * (barWidth + gap),
                          signal.bottom - px(4) - barHeight,
                          barsLeft + i * (barWidth + gap) + barWidth,
                          signal.bottom - px(4)};
            FillRect(dc, &barsRect, signalBrush);
        }
        DeleteObject(signalBrush);
        if (audio_.muted()) {
            // A slash across the bars: the machine is silent.
            drawLine(dc, signal.left, signal.bottom - px(4), signal.right,
                     signal.bottom - px(4), kBlack);
            drawLine(dc, signal.left, signal.bottom - px(4) - px(10),
                     signal.right, signal.bottom - px(4) + px(2), kWhite);
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

    void paintBezel(HDC dc, const MenuLayout& layout, int width, int height) const {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const RECT& outer = layout.monitor;
        const RECT& screen = layout.screen;
        if (outer.right - outer.left < px(20)) return;

        // The case around the glass: dark dust, a brighter lip, and a frame.
        fillDither(dc, RECT{outer.left, outer.top, outer.right, screen.top}, 6);
        fillDither(dc, RECT{outer.left, screen.bottom, outer.right,
                            outer.bottom}, 6);
        fillDither(dc, RECT{outer.left, screen.top, screen.left,
                            screen.bottom}, 6);
        fillDither(dc, RECT{screen.right, screen.top, outer.right,
                            screen.bottom}, 6);
        fillDither(dc, RECT{screen.left - px(4), screen.top - px(4),
                            screen.right + px(4), screen.top}, 18);
        fillDither(dc, RECT{screen.left - px(4), screen.bottom,
                            screen.right + px(4), screen.bottom + px(4)}, 18);
        fillDither(dc, RECT{screen.left - px(4), screen.top,
                            screen.left, screen.bottom}, 18);
        fillDither(dc, RECT{screen.right, screen.top, screen.right + px(4),
                            screen.bottom}, 18);

        strokeRect(dc, outer, kWhite, 2);
        strokeRect(dc, screen, kWhite, 1);

        // Case screws in the four corners of the bezel.
        auto screw = [&](int x, int y) {
            drawLine(dc, x - px(3), y, x + px(3), y);
            drawLine(dc, x, y - px(3), x, y + px(3));
        };
        screw(outer.left + px(11), outer.top + px(11));
        screw(outer.right - px(11), outer.top + px(11));
        screw(outer.left + px(11), outer.bottom - px(11));
        screw(outer.right - px(11), outer.bottom - px(11));

        const int textH = textHeight(dc, smallFont_);
        const int topBezel = screen.top - outer.top;
        drawTextCentered(dc, smallFont_, L"SOLVE ET COAGULA", width / 2,
                         outer.top + (topBezel - textH) / 2, kWhite);
        const int bottomBezel = outer.bottom - screen.bottom;
        drawTextCentered(dc, smallFont_, L"ANAISE  //  MODEL A-13", width / 2,
                         screen.bottom + (bottomBezel - textH) / 2, kWhite);

        // Vent slots flanking the name plate.
        HBRUSH vent = CreateSolidBrush(kWhite);
        const int slotY = outer.bottom - px(13);
        for (int i = 0; i < 6; ++i) {
            RECT leftSlot{outer.left + px(24) + i * px(7), slotY,
                          outer.left + px(24) + i * px(7) + px(4),
                          slotY + px(2)};
            RECT rightSlot{outer.right - px(28) - i * px(7), slotY,
                           outer.right - px(28) - i * px(7) + px(4),
                           slotY + px(2)};
            FillRect(dc, &leftSlot, vent);
            FillRect(dc, &rightSlot, vent);
        }
        DeleteObject(vent);

        // Power lamp, breathing so the machine looks awake.
        const int ledX = outer.right - px(44);
        const int ledY = screen.bottom + bottomBezel / 2;
        const int pulse = static_cast<int>(
            6.0 + 6.0 * (0.5 + 0.5 * std::sin(nowMs() * 0.0021)));
        fillDither(dc, RECT{ledX - px(5), ledY - px(5), ledX + px(5),
                            ledY + px(5)}, pulse);
        HBRUSH lamp = CreateSolidBrush(kWhite);
        const int ledSize = std::max(2, px(3));
        RECT led{ledX - ledSize / 2, ledY - ledSize / 2,
                 ledX - ledSize / 2 + ledSize, ledY - ledSize / 2 + ledSize};
        FillRect(dc, &led, lamp);
        DeleteObject(lamp);
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
        HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        SetBkMode(dc, TRANSPARENT);

        RECT all{0, 0, width, height};
        fillColor(dc, all, kBlack);

        // The seraphim owns the startup screen outright; every screen after it
        // hangs the slowly turning ring pattern behind the interface. The menu
        // lets the pattern run edge to edge inside the monitor glass, with only
        // the shell drawn over it; the other rooms fade it under their headers.
        if (screen_ != Screen::Splash) {
            if (screen_ == Screen::Menu) {
                if (!backgroundHidden_) {
                    background_.draw(dc, width, height, nowMs());
                }
            } else {
                const int scale = static_cast<int>(dpi_);
                const int logoCell = std::max(2, MulDiv(5, scale, 96));
                const int headerBand = MulDiv(34, scale, 96) +
                                       logoCell * kLogoGrid +
                                       MulDiv(26, scale, 96);
                const int footerBand = MulDiv(56, scale, 96);
                background_.draw(dc, width, height, nowMs(), headerBand,
                                 footerBand);
            }
        }

        ensureFonts();

        switch (screen_) {
            case Screen::Splash:
                paintSplash(dc, width, height);
                break;
            case Screen::Menu:
                paintMenu(dc, width, height);
                break;
            case Screen::Library:
                paintLibrary(dc, width, height);
                break;
            case Screen::Paint:
                paintPaintScreen(dc, width, height);
                break;
            case Screen::Browser:
                paintBrowser(dc, width, height);
                break;
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

            paintMenuBar(dc, layout);
            paintAppPanel(dc, layout);
            paintAppWindow(dc, layout);
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

    void drawRoom(HDC dc, const RoomLayout& room) const {
        const int cell = room.cell;
        const int width = cell * kRoomCols;
        const int height = cell * kRoomRows;
        const RECT roomRect{room.left, room.top, room.left + width,
                            room.top + height};
        fillColor(dc, roomRect, kBlack);
        const RECT outer = roomRect;
        strokeRect(dc, outer, kWhite, 1);

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
        if (rugInner.right > rugInner.left && rugInner.bottom > rugInner.top) {
            strokeRect(dc, rugInner, kWhite, 1);
        }

        // Shelving along the top wall, divided into one bay per tile.
        const int pad = std::max(2, cell / 14);
        for (int col = 1; col < kRoomCols; ++col) {
            const int x = room.left + col * cell;
            drawLine(dc, x, room.top + cell / 5, x, room.top + cell);
        }
        for (int col = 1; col < kRoomCols - 1; ++col) {
            drawShelfBooks(dc, room.left + col * cell + pad, room.top + pad * 2,
                           cell - pad * 2, cell - pad * 2, col);
        }

        // Bookcases standing on the floor.
        for (int row = 0; row < kRoomRows; ++row) {
            for (int col = 0; col < kRoomCols; ++col) {
                if (kRoomMap[row][col] != L'B') continue;
                const int frame = std::max(2, cell / 10);
                const int left = room.left + col * cell + frame;
                const int top = room.top + row * cell + frame;
                const int right = room.left + (col + 1) * cell - frame;
                const int bottom = room.top + (row + 1) * cell - frame;
                strokeRect(dc, RECT{left, top, right, bottom}, kWhite, 1);
                const int middle = (top + bottom) / 2;
                drawLine(dc, left + 1, middle, right - 1, middle);
                const int gap = std::max(1, cell / 20);
                const int bookTop = top + std::max(2, cell / 12);
                const int lowerTop = middle + std::max(2, cell / 12);
                drawShelfBooks(dc, left + 2, bookTop, right - left - 4,
                               middle - bookTop - gap, row * kRoomCols + col);
                drawShelfBooks(dc, left + 2, lowerTop, right - left - 4,
                               bottom - lowerTop - gap, col * 7 + row);
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
    }

    void paintPaintScreen(HDC dc, int width, int height) {
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int logoCell = std::max(2, MulDiv(4, scale, 96));
        int ruleY = 0;
        int titleY = 0;
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
        RECT bar{page.left, page.top, page.right, page.top + px(22)};
        drawMarquee(
            dc, bar,
            L"    WELCOME TO THE GAME    ///    YOU HAVE BEEN HERE BEFORE    ///    DO NOT REFRESH    ///    THE GAME IS PATIENT    ///    ");

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

        const int footerY = page.bottom - smallH;
        if (footerY > box.bottom + px(4)) {
            drawTextAt(dc, smallFont_,
                       L"LAST MODIFIED 06.06.1966   //   VISITORS 000000000",
                       page.left, footerY, kWhite);
        }
        // A caret that never settles.
        if ((nowMs() / 480) % 2 == 0) {
            const int caretX = page.right - px(14);
            const int caretY = page.bottom - smallH;
            fillColor(dc, RECT{caretX, caretY, caretX + px(7),
                               caretY + smallH},
                      kWhite);
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
        drawTextAt(dc, smallFont_,
                   L"HOST bael.veil   //   PAGE 01 OF 66   //   NO ENCRYPTION",
                   textLeft, y, kWhite);
        y += textHeight(dc, smallFont_) + px(8);
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
        RECT bar{page.left, page.top, split - px(12), page.top + px(22)};
        drawMarquee(dc, bar,
                    L"    THE ORDER OF THE UNLIT LAMP    ///    MEETINGS 13TH NIGHT    ///    NO ADDRESS    ///    ");
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
        drawTextAt(dc, smallFont_, L"HOST sabbatic-goat.veil   //   SOLVE",
                   textLeft, y, kWhite);
        y += textHeight(dc, smallFont_) + px(8);
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
        RECT bar{page.left, page.top, page.right, page.top + px(22)};
        drawMarquee(dc, bar, site.marquee ? site.marquee : L"");

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
        drawTextAt(dc, smallFont_, site.kicker, textLeft, y, kWhite);
        y += smallH + px(5);

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
        if (showChrome && site.badge && *site.badge) {
            const int badgeWidth =
                std::min(px(104), (columnWidth - px(4)) / 2);
            const int badgeTop = footerTop - px(3) - badgeH;
            drawWebBadge(dc, textLeft, badgeTop, badgeWidth, site.badge);
            if (columnWidth >= badgeWidth * 2 + px(4)) {
                drawWebBadge(dc, textLeft + badgeWidth + px(4), badgeTop,
                             badgeWidth, L"NO EXIT");
            }
        }
        if (showChrome) {
            drawTextAt(dc, smallFont_, site.footer, textLeft, footerTop,
                       kWhite);
            drawTextAt(dc, smallFont_,
                       L"< PREV   |   RANDOM   |   NEXT >   //   VEIL WEBRING",
                       textLeft, footerTop + footerLineH, kWhite);
            if (columnWidth >= px(430)) {
                drawHitCounter(dc, textRight - px(114), footerTop,
                               66600 + browserPage_ * 13, 6);
            }
        } else {
            drawTextAt(dc, smallFont_, site.footer, textLeft,
                       page.bottom - smallH, kWhite);
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
        RECT bar{page.left, page.top, page.right, page.top + px(22)};
        drawMarquee(dc, bar, site.marquee);

        const bool showArt =
            !compact && page.bottom - page.top >= px(300);
        const int split = page.left + (page.right - page.left) * 68 / 100;
        const int textRight = showArt ? split - px(14) : page.right;
        const int smallH = textHeight(dc, smallFont_);
        int y = bar.bottom + px(compact ? 8 : 12);
        drawTextAt(dc, smallFont_, site.kicker, page.left, y, kWhite);
        y += smallH + px(5);
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
        drawTextCentered(dc, headingFont_, L"CONTACTING HOST", center, y,
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
        drawTextCentered(dc, smallFont_,
                         L"WAITING FOR AN ANSWER THAT SHOULD NOT COME", center,
                         bar.bottom + px(8), kWhite);
        drawTextCentered(dc, smallFont_, L"IF THE HOST ANSWERS, DO NOT ANSWER BACK.",
                         center, bar.bottom + px(8) + textHeight(dc, smallFont_) + px(4),
                         kWhite);
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

    void paintBrowser(HDC dc, int width, int height) {
        const int scale = static_cast<int>(dpi_);
        auto px = [scale](int value) { return MulDiv(value, scale, 96); };
        const int margin = px(34);
        const int logoCell = std::max(2, px(5));
        int ruleY = 0;
        int titleY = 0;
        drawHeader(dc, width, L"VEIL", titleFont_, logoCell, ruleY, titleY);
        drawTextAt(dc, smallFont_, L"NETWORK  /  NO CARRIER  /  03:33",
                   width - margin, titleY, kWhite, TA_RIGHT | TA_TOP);

        const BrowserLayout layout = browserLayout(width, height);
        const RECT& window = layout.window;
        fillColor(dc, window, kBlack);
        strokeRect(dc, window, kWhite, 1);

        // Window title bar, with the page it is pretending to show.
        fillColor(dc, layout.title, kBlack);
        drawLine(dc, layout.title.left, layout.title.bottom - 1,
                 layout.title.right, layout.title.bottom - 1);
        const int iconCell = std::max(1, px(1));
        const int iconPixels = 16 * iconCell;
        const int iconLeft = layout.title.left + px(7);
        const int iconTop = layout.title.top +
                            (layout.title.bottom - layout.title.top -
                             iconPixels) / 2;
        drawGlyph(dc, appIconRows(2), iconLeft, iconTop, iconCell, kWhite);
        const int titleH = textHeight(dc, bodyFont_);
        const std::wstring windowTitle =
            std::wstring(L"VEIL.EXE  -  ") + kSites[browserPage_].title +
            L"  -  NOT SECURE";
        const int titleTextY = layout.title.top +
                               (layout.title.bottom - layout.title.top -
                                titleH) / 2;
        const int savedTitle = SaveDC(dc);
        IntersectClipRect(dc, iconLeft + iconPixels + px(8), layout.title.top,
                          layout.title.right - px(70), layout.title.bottom);
        drawTextAt(dc, bodyFont_, windowTitle,
                   iconLeft + iconPixels + px(8), titleTextY, kWhite);
        RestoreDC(dc, savedTitle);
        const int box = px(10);
        const int boxTop = layout.title.top +
                           (layout.title.bottom - layout.title.top - box) / 2;
        for (int i = 0; i < 3; ++i) {
            const int left = layout.title.right - px(7) - (i + 1) * px(14);
            RECT control{left, boxTop, left + box, boxTop + box};
            strokeRect(dc, control, kWhite, 1);
            if (i == 0) {
                drawLine(dc, control.left + px(2), control.top + px(2),
                         control.right - px(2), control.bottom - px(2));
                drawLine(dc, control.right - px(2), control.top + px(2),
                         control.left + px(2), control.bottom - px(2));
            }
        }

        // History tabs, which are the fifteen pages in the order they were found.
        for (int i = 0; i < kBrowserPages; ++i) {
            const RECT& tab = layout.tabs[i];
            if (i == browserPage_) {
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
            drawTextCentered(dc, smallFont_, kSites[i].tab,
                             (tab.left + tab.right) / 2,
                             tab.top + (tab.bottom - tab.top - tabH) / 2,
                             i == browserPage_ ? kBlack : kWhite);
            RestoreDC(dc, saved);
        }
        drawLine(dc, layout.window.left, layout.tabsBottom,
                 layout.window.right, layout.tabsBottom);

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
            drawLine(dc, cx - dir * reach, cy, cx, cy - reach);
            drawLine(dc, cx - dir * reach, cy, cx, cy + reach);
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
            std::wstring(L">  ") + kSites[browserPage_].url;
        const int addressH = textHeight(dc, smallFont_);
        drawTextAt(dc, smallFont_, addressText, layout.address.left + px(6),
                   layout.address.top +
                       (layout.address.bottom - layout.address.top - addressH) / 2,
                   kWhite);
        drawTextAt(dc, smallFont_, L"NO SIGNAL", layout.window.right - px(10),
                   layout.address.top +
                       (layout.address.bottom - layout.address.top - addressH) / 2,
                   kWhite, TA_RIGHT | TA_TOP);

        // Page.
        fillColor(dc, layout.content, kBlack);
        if (browserLoading()) {
            paintBrowserLoading(dc, layout);
        } else {
            RECT page = layout.content;
            InflateRect(&page, -px(20), -px(18));
            const RECT link = browserLinkRect(layout);
            page.bottom = link.top - px(8);
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
            paintBrowserLink(dc, layout);
        }

        // Status bar.
        fillColor(dc, layout.status, kBlack);
        drawLine(dc, layout.status.left, layout.status.top,
                 layout.status.right, layout.status.top);
        const int statusH = textHeight(dc, smallFont_);
        const int statusY =
            layout.status.top + (layout.status.bottom - layout.status.top -
                                 statusH) / 2;
        drawTextAt(dc, smallFont_,
                   browserLoading() ? L"CONTACTING..."
                                    : L"DONE (0.000 SECONDS)",
                   layout.status.left + px(8), statusY, kWhite);
        if (width >= px(820)) {
            drawTextCentered(dc, smallFont_,
                             L"COOKIES 13   //   SCRIPTS BLOCKED   //   WITNESSES 1",
                             (layout.status.left + layout.status.right) / 2,
                             statusY, kWhite);
        }
        drawTextAt(dc, smallFont_, L"03:33", layout.status.right - px(8),
                   statusY, kWhite, TA_RIGHT | TA_TOP);
    }

    // --------------------------------------------------------------- members
    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HANDLE embeddedFont_ = nullptr;
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
    UINT fontsDpi_ = 0;

    bool fullscreen_ = false;
    WINDOWPLACEMENT placement_{};
    LONG windowedStyle_ = 0;

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

    Walker walker_;
    ULONGLONG gameTickLast_ = 0;
    int lastDir_ = -1;

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
    if (!ensureDirectory(options.outDir)) return 1;

    static const Screen kScreens[] = {Screen::Splash, Screen::Menu,
                                      Screen::Library, Screen::Paint,
                                      Screen::Browser};
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
                                   options.browserPage)) {
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
