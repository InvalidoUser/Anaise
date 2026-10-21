// Anaise -- a small black-and-white library.
//
// A native Win32 app with a dot-art seraphim startup screen, a main menu, the
// quiet Empty Library room, and a pixel paint canvas. The palette is strictly
// black and white and nothing is written to disk.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "angel_art.h"

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

constexpr UINT_PTR kSplashTimer = 1;
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

class App {
public:
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
        return true;
    }

private:
    enum class Screen { Splash, Menu, Library, Paint };

    static constexpr int kMenuItems = 3;
    const wchar_t* const kMenuLabels[kMenuItems] = {
        L"THE EMPTY LIBRARY",
        L"PIXEL PAINT",
        L"QUIT",
    };

    struct GridLayout {
        int left = 0;
        int top = 0;
        int cell = 0;
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

            case WM_SIZE:
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;

            case WM_TIMER:
                if (wParam == kSplashTimer) {
                    tickSplash();
                    return 0;
                }
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
                    SetCursor(LoadCursorW(
                        nullptr,
                        screen_ == Screen::Paint ? IDC_CROSS : IDC_ARROW));
                    return TRUE;
                }
                break;

            case WM_MOUSEMOVE:
                onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;

            case WM_MOUSELEAVE:
                mouseTracking_ = false;
                if (hoverValid_) {
                    hoverValid_ = false;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;

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
                        screen_ == Screen::Library || screen_ == Screen::Paint) {
                        enterMenu();
                        return 0;
                    }
                    if (screen_ == Screen::Menu) {
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
                if (titleFont_) DeleteObject(titleFont_);
                if (headingFont_) DeleteObject(headingFont_);
                if (bodyFont_) DeleteObject(bodyFont_);
                if (smallFont_) DeleteObject(smallFont_);
                if (embeddedFont_) RemoveFontMemResourceEx(embeddedFont_);
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

    void enterMenu() {
        KillTimer(hwnd_, kSplashTimer);
        if (painting_) {
            painting_ = false;
            lastPaintValid_ = false;
            if (GetCapture() == hwnd_) ReleaseCapture();
        }
        screen_ = Screen::Menu;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void openScreen(Screen screen) {
        if (painting_) {
            painting_ = false;
            lastPaintValid_ = false;
            if (GetCapture() == hwnd_) ReleaseCapture();
        }
        screen_ = screen;
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
            default:
                DestroyWindow(hwnd_);
                break;
        }
    }

    void handleKey(int key) {
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
        if (screen_ == Screen::Paint) {
            if (key == 'C') {
                clearCanvas();
            } else if (key == '1' || key == '2' || key == '3') {
                paintBrush_ = key - '0';
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (key == 'Z' &&
                       (GetKeyState(VK_CONTROL) & 0x8000)) {
                undoCanvas();
            }
        }
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
            const int hit = menuIndexAt(x, y);
            if (hit >= 0 && hit != menuIndex_) {
                menuIndex_ = hit;
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
            const int hit = menuIndexAt(x, y);
            if (hit >= 0) {
                menuIndex_ = hit;
                InvalidateRect(hwnd_, nullptr, FALSE);
                activateMenuItem();
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

    int menuIndexAt(int x, int y) const {
        RECT client{};
        GetClientRect(hwnd_, &client);
        for (int i = 0; i < kMenuItems; ++i) {
            RECT item = menuItemRect(i, client.right, client.bottom);
            if (PtInRect(&item, POINT{x, y})) return i;
        }
        return -1;
    }

    RECT menuItemRect(int index, int width, int height) const {
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int itemWidth =
            std::min(width - margin * 2, MulDiv(460, scale, 96));
        const int itemHeight = MulDiv(56, scale, 96);
        const int gap = MulDiv(12, scale, 96);
        const int total = itemHeight * kMenuItems + gap * (kMenuItems - 1);
        const int logoCell = std::max(2, MulDiv(5, scale, 96));
        const int headerBottom =
            margin + logoCell * kLogoGrid + MulDiv(20, scale, 96) + 1;
        int startY = std::max(headerBottom + MulDiv(40, scale, 96),
                              (height - total) / 2 + MulDiv(10, scale, 96));
        const int maximumStart =
            height - MulDiv(112, scale, 96) - total;
        if (startY > maximumStart) startY = maximumStart;
        if (startY < headerBottom + MulDiv(12, scale, 96)) {
            startY = headerBottom + MulDiv(12, scale, 96);
        }
        const int left = (width - itemWidth) / 2;
        const int top = startY + index * (itemHeight + gap);
        return RECT{left, top, left + itemWidth, top + itemHeight};
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
    }

    void undoCanvas() {
        if (undo_.empty()) return;
        canvas_ = undo_.back();
        undo_.pop_back();
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
    void ensureFonts() {
        if (fontsDpi_ == dpi_ && titleFont_ && headingFont_ && bodyFont_ &&
            smallFont_) {
            return;
        }
        if (titleFont_) DeleteObject(titleFont_);
        if (headingFont_) DeleteObject(headingFont_);
        if (bodyFont_) DeleteObject(bodyFont_);
        if (smallFont_) DeleteObject(smallFont_);
        titleFont_ = nullptr;
        headingFont_ = nullptr;
        bodyFont_ = nullptr;
        smallFont_ = nullptr;

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

    void drawLine(HDC dc, int x1, int y1, int x2, int y2) const {
        HPEN pen = CreatePen(PS_SOLID, 1, kWhite);
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, x1, y1, nullptr);
        LineTo(dc, x2, y2);
        SelectObject(dc, old);
        DeleteObject(pen);
    }

    void drawLogo(HDC dc, int left, int top, int cell) const {
        HBRUSH brush = CreateSolidBrush(kWhite);
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
        const int width = client.right;
        const int height = client.bottom;
        if (width <= 0 || height <= 0) return;

        HDC dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        SetBkMode(dc, TRANSPARENT);

        RECT all{0, 0, width, height};
        fillColor(dc, all, kBlack);

        ensureFonts();

        switch (screen_) {
            case Screen::Splash:
                paintSplash(dc, width, height);
                break;
            case Screen::Menu:
                paintMenu(dc, width, height);
                break;
            case Screen::Library:
                paintLibrary(dc, width);
                break;
            case Screen::Paint:
                paintPaintScreen(dc, width, height);
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

        const ULONGLONG elapsed = GetTickCount64() - splashStart_;
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
        const int scale = static_cast<int>(dpi_);
        const int logoCell = std::max(2, MulDiv(5, scale, 96));
        int ruleY = 0;
        int titleY = 0;
        drawHeader(dc, width, L"ANAISE", titleFont_, logoCell, ruleY, titleY);

        for (int i = 0; i < kMenuItems; ++i) {
            const RECT item = menuItemRect(i, width, height);
            const std::wstring label = kMenuLabels[i];
            const int labelHeight = textHeight(dc, bodyFont_);
            const int labelY =
                item.top + (item.bottom - item.top - labelHeight) / 2;
            if (i == menuIndex_) {
                fillColor(dc, item, kWhite);
                drawTextCentered(dc, bodyFont_, label, (item.left + item.right) / 2,
                                 labelY, kBlack);
            } else {
                strokeRect(dc, item, kWhite, 1);
                drawTextCentered(dc, bodyFont_, label, (item.left + item.right) / 2,
                                 labelY, kWhite);
            }
        }
    }

    void paintLibrary(HDC dc, int width) {
        const int scale = static_cast<int>(dpi_);
        const int logoCell = std::max(2, MulDiv(5, scale, 96));
        int ruleY = 0;
        int titleY = 0;
        drawHeader(dc, width, L"THE EMPTY LIBRARY", titleFont_, logoCell, ruleY,
                   titleY);
    }

    void paintPaintScreen(HDC dc, int width, int height) {
        const int scale = static_cast<int>(dpi_);
        const int margin = MulDiv(34, scale, 96);
        const int logoCell = std::max(2, MulDiv(4, scale, 96));
        int ruleY = 0;
        int titleY = 0;
        drawHeader(dc, width, L"PIXEL PAINT", headingFont_, logoCell, ruleY,
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

    // --------------------------------------------------------------- members
    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HANDLE embeddedFont_ = nullptr;

    UINT dpi_ = 96;
    HFONT titleFont_ = nullptr;
    HFONT headingFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    UINT fontsDpi_ = 0;

    bool fullscreen_ = false;
    WINDOWPLACEMENT placement_{};
    LONG windowedStyle_ = 0;

    Screen screen_ = Screen::Splash;
    ULONGLONG splashStart_ = 0;
    int menuIndex_ = 0;

    std::array<std::array<bool, kPaintCols>, kPaintRows> canvas_{};
    std::vector<std::array<std::array<bool, kPaintCols>, kPaintRows>> undo_;
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

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    enableDpiAwareness();

    App app;
    if (!app.create(instance)) return 1;

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
