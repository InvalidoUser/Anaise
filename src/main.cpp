// The Hollow Spire -- an ASCII text adventure in a native Win32 window.
//
// The window is deliberately ordinary: a real frame, a real menu, F11 and
// Alt+Enter fullscreen, Alt+F4 to close, resizable, DPI aware, with a dark
// title bar. Everything inside is drawn by hand with GDI: a scrolling story,
// a status line, and one line of input.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "game.h"

namespace {

enum MenuId : int {
    ID_GAME_NEW = 1001,
    ID_GAME_SAVE,
    ID_GAME_LOAD,
    ID_GAME_EXIT,
    ID_VIEW_FULLSCREEN,
    ID_VIEW_FONT_INC,
    ID_VIEW_FONT_DEC,
    ID_VIEW_CLEAR,
    ID_HELP_COMMANDS,
    ID_HELP_ABOUT,
};

constexpr UINT_PTR kCaretTimer = 1;
constexpr UINT_PTR kFlashTimer = 2;
constexpr UINT kCaretIntervalMs = 530;

const COLORREF kBgTop = RGB(15, 19, 26);
const COLORREF kBgBottom = RGB(7, 9, 12);
const COLORREF kPanel = RGB(15, 19, 25);
const COLORREF kBorder = RGB(43, 49, 58);
const COLORREF kBorderBright = RGB(96, 78, 49);

const COLORREF kInkText = RGB(209, 201, 184);
const COLORREF kInkAccent = RGB(233, 184, 110);
const COLORREF kInkRoom = RGB(240, 214, 166);
const COLORREF kInkArt = RGB(120, 104, 78);
const COLORREF kInkDim = RGB(122, 125, 131);
const COLORREF kInkItem = RGB(154, 199, 144);
const COLORREF kInkGood = RGB(141, 201, 160);
const COLORREF kInkBad = RGB(226, 127, 107);
const COLORREF kInkSystem = RGB(124, 160, 186);
const COLORREF kInkCommand = RGB(228, 224, 212);

std::wstring lowerAscii(std::wstring value) {
    for (wchar_t& ch : value) {
        if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return value;
}

std::wstring trimCopy(const std::wstring& value) {
    size_t first = 0;
    while (first < value.size() &&
           (value[first] == L' ' || value[first] == L'\t')) {
        ++first;
    }
    size_t last = value.size();
    while (last > first &&
           (value[last - 1] == L' ' || value[last - 1] == L'\t')) {
        --last;
    }
    return value.substr(first, last - first);
}

COLORREF inkColor(Ink ink) {
    switch (ink) {
        case Ink::Accent: return kInkAccent;
        case Ink::Room: return kInkRoom;
        case Ink::Art: return kInkArt;
        case Ink::Dim: return kInkDim;
        case Ink::Item: return kInkItem;
        case Ink::Good: return kInkGood;
        case Ink::Bad: return kInkBad;
        case Ink::System: return kInkSystem;
        case Ink::Command: return kInkCommand;
        case Ink::Text:
        default: return kInkText;
    }
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

void enableDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    auto fn = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (fn) {
        const HANDLE perMonitorV2 = reinterpret_cast<HANDLE>(
            static_cast<LONG_PTR>(-4));
        fn(perMonitorV2);
    }
}

std::wstring settingsPath() {
    const wchar_t* appData = _wgetenv(L"APPDATA");
    std::wstring base = appData && *appData ? appData
                                            : L".";
    return base + L"\\HollowSpire\\settings.ini";
}

void applyDarkTitleBar(HWND hwnd) {
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
        COLORREF caption = RGB(13, 16, 21);
        fn(hwnd, 35, &caption, sizeof(caption));
        COLORREF text = RGB(215, 206, 188);
        fn(hwnd, 36, &text, sizeof(text));
        COLORREF border = RGB(43, 49, 58);
        fn(hwnd, 34, &border, sizeof(border));
    }
    FreeLibrary(dwm);
}

struct Cell {
    wchar_t ch;
    Ink ink;
};

struct OutRun {
    std::wstring text;
    Ink ink;
};

struct VisualLine {
    std::vector<OutRun> runs;
    bool art = false;
};

class App {
public:
    App() : game_([](const Block& block) { (void)block; }) {}

    bool create(HINSTANCE instance) {
        instance_ = instance;
        game_ = Game([this](const Block& block) { log_.push_back(block); });

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &App::windowProc;
        wc.hInstance = instance;
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
        wc.hIconSm = reinterpret_cast<HICON>(
            LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
                       GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                       LR_DEFAULTCOLOR));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"HollowSpireWindow";
        if (!RegisterClassExW(&wc)) return false;

        menu_ = buildMenu();

        int width = 1240;
        int height = 860;
        int x = CW_USEDEFAULT;
        int y = CW_USEDEFAULT;
        int maximized = 0;

        std::wstring ini = settingsPath();
        fontPt_ = std::max(9, std::min(20,
            static_cast<int>(GetPrivateProfileIntW(L"ui", L"font", 12, ini.c_str()))));
        int savedWidth = static_cast<int>(GetPrivateProfileIntW(L"window", L"w", 0, ini.c_str()));
        int savedHeight = static_cast<int>(GetPrivateProfileIntW(L"window", L"h", 0, ini.c_str()));
        int savedX = static_cast<int>(GetPrivateProfileIntW(L"window", L"x", -32000, ini.c_str()));
        int savedY = static_cast<int>(GetPrivateProfileIntW(L"window", L"y", -32000, ini.c_str()));
        maximized = GetPrivateProfileIntW(L"window", L"maximized", 0, ini.c_str());
        if (savedWidth >= 640 && savedHeight >= 420) {
            RECT test{savedX, savedY, savedX + savedWidth, savedY + savedHeight};
            if (MonitorFromRect(&test, MONITOR_DEFAULTTONULL) != nullptr) {
                width = savedWidth;
                height = savedHeight;
                x = savedX;
                y = savedY;
            }
        }

        hwnd_ = CreateWindowExW(
            0, wc.lpszClassName, L"The Hollow Spire",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, width, height, nullptr,
            menu_, instance, this);
        if (!hwnd_) return false;

        applyDarkTitleBar(hwnd_);
        if (maximized) {
            ShowWindow(hwnd_, SW_SHOWMAXIMIZED);
        } else {
            ShowWindow(hwnd_, SW_SHOW);
        }
        UpdateWindow(hwnd_);

        dpi_ = windowDpi(hwnd_);
        game_.newGame();
        updateTitle();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

private:
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

    HMENU buildMenu() {
        HMENU root = CreateMenu();

        HMENU game = CreatePopupMenu();
        AppendMenuW(game, MF_STRING, ID_GAME_NEW, L"&New Game\tF2");
        AppendMenuW(game, MF_STRING, ID_GAME_SAVE, L"&Save Chronicle\tF5");
        AppendMenuW(game, MF_STRING, ID_GAME_LOAD, L"&Load Chronicle\tF9");
        AppendMenuW(game, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(game, MF_STRING, ID_GAME_EXIT, L"E&xit\tAlt+F4");
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(game), L"&Game");

        HMENU view = CreatePopupMenu();
        AppendMenuW(view, MF_STRING, ID_VIEW_FULLSCREEN, L"&Fullscreen\tF11");
        AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(view, MF_STRING, ID_VIEW_FONT_INC, L"Larger Text\tCtrl+Plus");
        AppendMenuW(view, MF_STRING, ID_VIEW_FONT_DEC, L"Smaller Text\tCtrl+Minus");
        AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(view, MF_STRING, ID_VIEW_CLEAR, L"&Clear Scrollback\tCtrl+L");
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"&View");

        HMENU help = CreatePopupMenu();
        AppendMenuW(help, MF_STRING, ID_HELP_COMMANDS, L"&Commands\tF1");
        AppendMenuW(help, MF_STRING, ID_HELP_ABOUT, L"&About");
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Help");

        return root;
    }

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_CREATE:
                SetMenu(hwnd_, menu_);
                return 0;

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
                info->ptMinTrackSize.x = MulDiv(860, dpi, 96);
                info->ptMinTrackSize.y = MulDiv(560, dpi, 96);
                return 0;
            }

            case WM_SETFOCUS:
                focused_ = true;
                caretOn_ = true;
                SetTimer(hwnd_, kCaretTimer, kCaretIntervalMs, nullptr);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;

            case WM_KILLFOCUS:
                focused_ = false;
                caretOn_ = false;
                KillTimer(hwnd_, kCaretTimer);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;

            case WM_TIMER:
                if (wParam == kCaretTimer) {
                    caretOn_ = !caretOn_;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                } else if (wParam == kFlashTimer) {
                    if (GetTickCount64() > flashUntil_) {
                        KillTimer(hwnd_, kFlashTimer);
                        flash_.clear();
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;

            case WM_CHAR:
                onChar(static_cast<wchar_t>(wParam));
                return 0;

            case WM_KEYDOWN:
                onKeyDown(wParam, lParam);
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

            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTCLIENT) {
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(hwnd_, &pt);
                    if (PtInRect(&inputRect_, pt)) {
                        SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                        return TRUE;
                    }
                }
                break;

            case WM_LBUTTONDOWN: {
                SetFocus(hwnd_);
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                POINT pt{x, y};
                if (totalLines_ > visibleLines_ &&
                    PtInRect(&scrollTrackHit_, pt)) {
                    draggingScroll_ = true;
                    SetCapture(hwnd_);
                    scrollToThumbY(y);
                    return 0;
                }
                if (PtInRect(&inputRect_, pt) && cellW_ > 0) {
                    int inner = x - inputTextX_;
                    int index = inputViewStart_ + inner / cellW_;
                    if (index < 0) index = 0;
                    if (index > static_cast<int>(input_.size())) {
                        index = static_cast<int>(input_.size());
                    }
                    caret_ = static_cast<size_t>(index);
                    caretOn_ = true;
                    tabActive_ = false;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;
            }

            case WM_MOUSEMOVE:
                if (draggingScroll_) {
                    scrollToThumbY(GET_Y_LPARAM(lParam));
                    return 0;
                }
                break;

            case WM_LBUTTONUP:
                if (draggingScroll_) {
                    draggingScroll_ = false;
                    ReleaseCapture();
                    return 0;
                }
                break;

            case WM_CAPTURECHANGED:
                draggingScroll_ = false;
                break;

            case WM_MOUSEWHEEL: {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int lines = delta / WHEEL_DELTA * 3;
                scroll_ = std::max(0, std::min(maxScroll_, scroll_ + lines));
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }

            case WM_COMMAND:
                onCommand(LOWORD(wParam));
                return 0;

            case WM_CLOSE:
                DestroyWindow(hwnd_);
                return 0;

            case WM_DESTROY:
                saveSettings();
                KillTimer(hwnd_, kCaretTimer);
                KillTimer(hwnd_, kFlashTimer);
                if (font_) DeleteObject(font_);
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd_, message, wParam, lParam);
    }

    // --------------------------------------------------------------- command
    void onCommand(int id) {
        switch (id) {
            case ID_GAME_NEW:
                requestNewGame();
                break;
            case ID_GAME_SAVE:
                game_.save();
                scroll_ = 0;
                InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            case ID_GAME_LOAD:
                game_.load();
                scroll_ = 0;
                updateTitle();
                InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            case ID_GAME_EXIT:
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
                break;
            case ID_VIEW_FULLSCREEN:
                setFullscreen(!fullscreen_);
                break;
            case ID_VIEW_FONT_INC:
                changeFont(1);
                break;
            case ID_VIEW_FONT_DEC:
                changeFont(-1);
                break;
            case ID_VIEW_CLEAR:
                clearScrollback();
                break;
            case ID_HELP_COMMANDS:
                game_.showHelp();
                scroll_ = 0;
                InvalidateRect(hwnd_, nullptr, FALSE);
                break;
            case ID_HELP_ABOUT:
                MessageBoxW(
                    hwnd_,
                    L"THE HOLLOW SPIRE\n"
                    L"An ASCII text adventure in a native Windows window.\n\n"
                    L"C++ and the Win32 API, with GDI for every letter.\n"
                    L"No engine, no runtime, no network.\n\n"
                    L"F11 or Alt+Enter: fullscreen\n"
                    L"F1: commands    F5: save    F9: load\n"
                    L"Alt+F4: close",
                    L"About The Hollow Spire",
                    MB_OK | MB_ICONINFORMATION);
                break;
        }
    }

    // ----------------------------------------------------------------- input
    void onChar(wchar_t ch) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t' || ch == 27 ||
            ch == 8) {
            return;
        }
        if (ch < 32 || ch > 126) return;
        if (input_.size() >= 220) return;
        input_.insert(input_.begin() + static_cast<long>(caret_), ch);
        ++caret_;
        caretOn_ = true;
        tabActive_ = false;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void onKeyDown(WPARAM key, LPARAM lParam) {
        const bool repeat = (lParam & (1 << 30)) != 0;
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        switch (key) {
            case VK_F11:
                if (!repeat) setFullscreen(!fullscreen_);
                return;
            case VK_F1:
                if (!repeat) {
                    game_.showHelp();
                    scroll_ = 0;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return;
            case VK_F2:
                if (!repeat) requestNewGame();
                return;
            case VK_F5:
                if (!repeat) {
                    game_.save();
                    scroll_ = 0;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return;
            case VK_F9:
                if (!repeat) {
                    game_.load();
                    scroll_ = 0;
                    updateTitle();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return;
            case VK_ESCAPE:
                if (fullscreen_) {
                    setFullscreen(false);
                } else if (!input_.empty()) {
                    input_.clear();
                    caret_ = 0;
                    inputViewStart_ = 0;
                    tabActive_ = false;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return;
            case VK_RETURN:
                submitInput();
                return;
            case VK_BACK:
                if (caret_ > 0) {
                    size_t count = 1;
                    if (ctrl) {
                        while (caret_ - count > 0 &&
                               input_[caret_ - count - 1] == L' ') {
                            ++count;
                        }
                        while (caret_ - count > 0 &&
                               input_[caret_ - count - 1] != L' ') {
                            ++count;
                        }
                    }
                    input_.erase(caret_ - count, count);
                    caret_ -= count;
                }
                tabActive_ = false;
                caretOn_ = true;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_DELETE:
                if (caret_ < input_.size()) {
                    input_.erase(caret_, 1);
                }
                tabActive_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_LEFT:
                if (ctrl) {
                    while (caret_ > 0 && input_[caret_ - 1] == L' ') --caret_;
                    while (caret_ > 0 && input_[caret_ - 1] != L' ') --caret_;
                } else if (caret_ > 0) {
                    --caret_;
                }
                caretOn_ = true;
                tabActive_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_RIGHT:
                if (caret_ < input_.size()) ++caret_;
                caretOn_ = true;
                tabActive_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_HOME:
                if (ctrl) {
                    scroll_ = maxScroll_;
                } else {
                    caret_ = 0;
                }
                caretOn_ = true;
                tabActive_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_END:
                if (ctrl) {
                    scroll_ = 0;
                } else {
                    caret_ = input_.size();
                }
                caretOn_ = true;
                tabActive_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_PRIOR:
                scroll_ = std::min(maxScroll_, scroll_ + visibleLines_ - 2);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_NEXT:
                scroll_ = std::max(0, scroll_ - (visibleLines_ - 2));
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            case VK_UP:
                historyMove(-1);
                return;
            case VK_DOWN:
                historyMove(1);
                return;
            case VK_TAB:
                completeInput();
                return;
            case VK_INSERT:
                if (shift || ctrl) pasteClipboard();
                return;
            case 'C':
                if (ctrl) copyTranscript();
                return;
            case 'V':
                if (ctrl) pasteClipboard();
                return;
            case 'L':
                if (ctrl) clearScrollback();
                return;
            case VK_OEM_PLUS:
            case VK_ADD:
                if (ctrl) changeFont(1);
                return;
            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                if (ctrl) changeFont(-1);
                return;
        }
    }

    void submitInput() {
        std::wstring command = trimCopy(input_);
        if (command.empty()) {
            input_.clear();
            caret_ = 0;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        Block echo;
        echo.spans.push_back({L"> ", Ink::Accent});
        echo.spans.push_back({command, Ink::Command});
        log_.push_back(echo);

        addHistory(command);
        historyPos_ = -1;
        draft_.clear();
        input_.clear();
        caret_ = 0;
        inputViewStart_ = 0;
        tabActive_ = false;
        scroll_ = 0;

        Action action = game_.submit(command);
        if (action == Action::NewGame) {
            if (!requestNewGame()) {
                Block note;
                note.spans.push_back({L"New game cancelled.", Ink::Dim});
                log_.push_back(note);
            }
        } else if (action == Action::Quit) {
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        } else if (action == Action::Clear) {
            clearScrollback();
        }

        scroll_ = 0;
        updateTitle();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // ------------------------------------------------------------- new game
    bool requestNewGame() {
        if (!game_.gameOver() && game_.moves() > 0) {
            int result = MessageBoxW(
                hwnd_, L"Abandon this chronicle and begin again?",
                L"The Hollow Spire", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
            if (result != IDYES) return false;
        }
        log_.clear();
        input_.clear();
        caret_ = 0;
        inputViewStart_ = 0;
        scroll_ = 0;
        historyPos_ = -1;
        game_.newGame();
        updateTitle();
        SetFocus(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    void clearScrollback() {
        log_.clear();
        Block note;
        note.spans.push_back(
            {L"The scroll is wiped clean. Type LOOK to see where you are.",
             Ink::Dim});
        log_.push_back(note);
        scroll_ = 0;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void changeFont(int delta) {
        fontPt_ = std::max(9, std::min(20, fontPt_ + delta));
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void updateTitle() {
        std::wstring title = L"The Hollow Spire";
        if (game_.gameOver()) {
            title += L" - The Story Is Ended";
        } else {
            title += L" - " + game_.location();
        }
        SetWindowTextW(hwnd_, title.c_str());
    }

    // ------------------------------------------------------ clipboard & tab
    void copyTranscript() {
        std::wstring text;
        for (const auto& block : log_) {
            for (const auto& span : block.spans) text += span.text;
            text += L"\r\n";
        }
        if (text.empty()) return;
        if (!OpenClipboard(hwnd_)) return;
        EmptyClipboard();
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory) {
            if (void* target = GlobalLock(memory)) {
                std::memcpy(target, text.c_str(), bytes);
                GlobalUnlock(memory);
                if (!SetClipboardData(CF_UNICODETEXT, memory)) {
                    GlobalFree(memory);
                }
            } else {
                GlobalFree(memory);
            }
        }
        CloseClipboard();
        flash(L"transcript copied to clipboard");
    }

    void pasteClipboard() {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return;
        if (!OpenClipboard(hwnd_)) return;
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        std::wstring text;
        if (handle) {
            if (const wchar_t* source =
                    static_cast<const wchar_t*>(GlobalLock(handle))) {
                for (const wchar_t* p = source; *p && text.size() < 220; ++p) {
                    wchar_t ch = *p;
                    if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
                    if (ch >= 32 && ch <= 126) text.push_back(ch);
                }
                GlobalUnlock(handle);
            }
        }
        CloseClipboard();
        if (text.empty()) return;
        input_.insert(caret_, text);
        caret_ += text.size();
        caretOn_ = true;
        tabActive_ = false;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void completeInput() {
        auto isWordChar = [](wchar_t ch) {
            return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                   (ch >= L'0' && ch <= L'9') || ch == L'\'';
        };
        size_t start = caret_;
        while (start > 0 && isWordChar(input_[start - 1])) --start;
        if (start == caret_) return;

        std::wstring prefix = lowerAscii(input_.substr(start, caret_ - start));
        bool cycling = false;
        if (tabActive_ && tabStart_ == start && !tabCandidates_.empty()) {
            std::wstring current = lowerAscii(input_.substr(start, caret_ - start));
            size_t last = (tabIndex_ + tabCandidates_.size() - 1) %
                          tabCandidates_.size();
            if (current == lowerAscii(tabCandidates_[last])) {
                cycling = true;
            }
        }
        if (!cycling) {
            tabCandidates_.clear();
            for (const auto& word : game_.completerWords()) {
                if (word.size() >= prefix.size() &&
                    lowerAscii(word).compare(0, prefix.size(), prefix) == 0) {
                    tabCandidates_.push_back(word);
                }
            }
            std::sort(tabCandidates_.begin(), tabCandidates_.end());
            tabCandidates_.erase(
                std::unique(tabCandidates_.begin(), tabCandidates_.end()),
                tabCandidates_.end());
            tabStart_ = start;
            tabPrefix_ = prefix;
            tabIndex_ = 0;
            tabActive_ = !tabCandidates_.empty();
            if (tabCandidates_.empty()) return;
        }

        std::wstring chosen = tabCandidates_[tabIndex_ % tabCandidates_.size()];
        input_.replace(start, caret_ - start, chosen);
        caret_ = start + chosen.size();
        tabIndex_ = (tabIndex_ + 1) % tabCandidates_.size();
        caretOn_ = true;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // -------------------------------------------------------------- history
    void addHistory(const std::wstring& command) {
        if (!history_.empty() && history_.back() == command) return;
        history_.push_back(command);
        if (history_.size() > 200) history_.erase(history_.begin());
    }

    void historyMove(int direction) {
        if (history_.empty()) return;
        if (historyPos_ == -1) {
            draft_ = input_;
            historyPos_ = direction < 0 ? static_cast<int>(history_.size()) - 1
                                        : -1;
            if (historyPos_ == -1) return;
        } else {
            historyPos_ += direction;
            if (historyPos_ >= static_cast<int>(history_.size())) {
                historyPos_ = -1;
                input_ = draft_;
                caret_ = input_.size();
                tabActive_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
            if (historyPos_ < 0) historyPos_ = 0;
        }
        input_ = history_[historyPos_];
        caret_ = input_.size();
        tabActive_ = false;
        caretOn_ = true;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // ------------------------------------------------------------- flashing
    void flash(const std::wstring& text) {
        flash_ = text;
        flashUntil_ = GetTickCount64() + 1600;
        SetTimer(hwnd_, kFlashTimer, 200, nullptr);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void scrollToThumbY(int y) {
        int travel = scrollTrackHeight_ - scrollThumbHeight_;
        if (travel <= 0 || maxScroll_ <= 0) return;
        int offset = y - storyTop_ - scrollThumbHeight_ / 2;
        offset = std::max(0, std::min(travel, offset));
        scroll_ = maxScroll_ - (maxScroll_ * offset + travel / 2) / travel;
        scroll_ = std::max(0, std::min(maxScroll_, scroll_));
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // ------------------------------------------------------------- settings
    void saveSettings() {
        std::wstring ini = settingsPath();
        std::wstring folder = ini.substr(0, ini.find_last_of(L'\\'));
        CreateDirectoryW(folder.c_str(), nullptr);

        WINDOWPLACEMENT placement{};
        if (fullscreen_ && placement_.length) {
            placement = placement_;
        } else {
            placement.length = sizeof(placement);
            GetWindowPlacement(hwnd_, &placement);
        }
        bool maximized = placement.showCmd == SW_SHOWMAXIMIZED;
        RECT rect = placement.rcNormalPosition;

        std::wstring buffer;
        buffer = std::to_wstring(rect.left);
        WritePrivateProfileStringW(L"window", L"x", buffer.c_str(), ini.c_str());
        buffer = std::to_wstring(rect.top);
        WritePrivateProfileStringW(L"window", L"y", buffer.c_str(), ini.c_str());
        buffer = std::to_wstring(rect.right - rect.left);
        WritePrivateProfileStringW(L"window", L"w", buffer.c_str(), ini.c_str());
        buffer = std::to_wstring(rect.bottom - rect.top);
        WritePrivateProfileStringW(L"window", L"h", buffer.c_str(), ini.c_str());
        WritePrivateProfileStringW(L"window", L"maximized",
                                   maximized ? L"1" : L"0", ini.c_str());
        buffer = std::to_wstring(fontPt_);
        WritePrivateProfileStringW(L"ui", L"font", buffer.c_str(), ini.c_str());
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
            SetMenu(hwnd_, nullptr);
            SetWindowLongPtrW(hwnd_, GWL_STYLE,
                              WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
            SetWindowPos(hwnd_, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
                         info.rcMonitor.right - info.rcMonitor.left,
                         info.rcMonitor.bottom - info.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            fullscreen_ = true;
        } else {
            SetWindowLongPtrW(hwnd_, GWL_STYLE, windowedStyle_);
            SetMenu(hwnd_, menu_);
            SetWindowPlacement(hwnd_, &placement_);
            SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                             SWP_FRAMECHANGED);
            DrawMenuBar(hwnd_);
            fullscreen_ = false;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // --------------------------------------------------------------- drawing
    void ensureFont(HDC dc) {
        int effectivePt = effectiveFontPt();
        if (font_ && fontDpi_ == dpi_ && fontPtCached_ == effectivePt) return;
        if (font_) {
            DeleteObject(font_);
            font_ = nullptr;
        }
        LOGFONTW lf{};
        lf.lfHeight = -MulDiv(effectivePt, static_cast<int>(dpi_), 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        lstrcpyW(lf.lfFaceName, L"Consolas");
        font_ = CreateFontIndirectW(&lf);
        if (!font_) {
            lstrcpyW(lf.lfFaceName, L"Lucida Console");
            font_ = CreateFontIndirectW(&lf);
        }
        fontDpi_ = dpi_;
        fontPtCached_ = effectivePt;
    }

    int effectiveFontPt() const {
        int pt = fontPt_ + (fullscreen_ ? 4 : 0);
        return std::max(9, std::min(26, pt));
    }

    SIZE measure(HDC dc, const std::wstring& text) const {
        SIZE size{};
        if (!text.empty()) {
            GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()),
                                  &size);
        }
        return size;
    }

    void drawText(HDC dc, int x, int y, const std::wstring& text,
                  COLORREF color) {
        if (text.empty()) return;
        SetTextColor(dc, color);
        TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    }

    void drawRight(HDC dc, int right, int y, const std::wstring& text,
                   COLORREF color) {
        SIZE size = measure(dc, text);
        drawText(dc, right - size.cx, y, text, color);
    }

    void fillRectColor(HDC dc, const RECT& rect, COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    void drawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, x1, y1, nullptr);
        LineTo(dc, x2, y2);
        SelectObject(dc, old);
        DeleteObject(pen);
    }

    std::vector<VisualLine> wrapBlocks(int cols) {
        std::vector<VisualLine> lines;
        if (cols < 8) cols = 8;

        auto blockLength = [](const Block& block) {
            size_t length = 0;
            for (const auto& span : block.spans) length += span.text.size();
            return length;
        };

        size_t index = 0;
        while (index < log_.size()) {
            if (log_[index].art) {
                size_t end = index;
                size_t widest = 0;
                while (end < log_.size() && log_[end].art) {
                    widest = std::max(widest, blockLength(log_[end]));
                    ++end;
                }
                int indent = 0;
                if (static_cast<int>(widest) < cols) {
                    indent = (cols - static_cast<int>(widest)) / 2;
                }
                for (size_t i = index; i < end; ++i) {
                    VisualLine line;
                    line.art = true;
                    std::wstring text(static_cast<size_t>(indent), L' ');
                    for (const auto& span : log_[i].spans) text += span.text;
                    if (!text.empty()) line.runs.push_back({text, Ink::Art});
                    lines.push_back(std::move(line));
                }
                index = end;
                continue;
            }

            std::vector<Cell> cells;
            for (const auto& span : log_[index].spans) {
                for (wchar_t ch : span.text) cells.push_back({ch, span.ink});
            }
            if (cells.empty()) {
                lines.push_back(VisualLine{});
                ++index;
                continue;
            }

            size_t position = 0;
            bool firstSegment = true;
            while (position < cells.size()) {
                if (!firstSegment) {
                    while (position < cells.size() && cells[position].ch == L' ') {
                        ++position;
                    }
                    if (position >= cells.size()) break;
                }

                size_t remaining = cells.size() - position;
                size_t lineEnd = 0;
                if (remaining <= static_cast<size_t>(cols)) {
                    lineEnd = cells.size();
                } else {
                    size_t limit = position + static_cast<size_t>(cols);
                    size_t breakAt = limit;
                    while (breakAt > position && cells[breakAt - 1].ch != L' ') {
                        --breakAt;
                    }
                    lineEnd = breakAt > position ? breakAt : limit;
                }

                size_t contentEnd = lineEnd;
                while (contentEnd > position && cells[contentEnd - 1].ch == L' ') {
                    --contentEnd;
                }
                if (contentEnd == position && lineEnd > position) {
                    contentEnd = position + 1;
                }

                VisualLine line;
                for (size_t i = position; i < contentEnd; ++i) {
                    if (!line.runs.empty() && line.runs.back().ink == cells[i].ink) {
                        line.runs.back().text.push_back(cells[i].ch);
                    } else {
                        line.runs.push_back({std::wstring(1, cells[i].ch),
                                             cells[i].ink});
                    }
                }
                lines.push_back(std::move(line));

                position = lineEnd;
                while (position < cells.size() && cells[position].ch == L' ') {
                    ++position;
                }
                firstSegment = false;
            }
            ++index;
        }
        return lines;
    }

    void paint(HDC screen) {
        RECT client{};
        GetClientRect(hwnd_, &client);
        int width = client.right;
        int height = client.bottom;
        if (width <= 0 || height <= 0) return;

        HDC dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        SetBkMode(dc, TRANSPARENT);

        TRIVERTEX vertices[2] = {
            {0, 0, static_cast<COLOR16>(GetRValue(kBgTop) << 8),
             static_cast<COLOR16>(GetGValue(kBgTop) << 8),
             static_cast<COLOR16>(GetBValue(kBgTop) << 8), 0},
            {width, height, static_cast<COLOR16>(GetRValue(kBgBottom) << 8),
             static_cast<COLOR16>(GetGValue(kBgBottom) << 8),
             static_cast<COLOR16>(GetBValue(kBgBottom) << 8), 0}};
        GRADIENT_RECT gradient{0, 1};
        if (!GradientFill(dc, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_V)) {
            RECT all{0, 0, width, height};
            fillRectColor(dc, all, kBgBottom);
        }

        ensureFont(dc);
        HGDIOBJ oldFont = SelectObject(dc, font_);

        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);
        cellH_ = metrics.tmHeight + MulDiv(5, static_cast<int>(dpi_), 96);
        SIZE cell = measure(dc, L"M");
        cellW_ = std::max(1, static_cast<int>(cell.cx));

        const int scale = MulDiv(1, static_cast<int>(dpi_), 96);
        const int margin = MulDiv(26, static_cast<int>(dpi_), 96);

        int contentW = width - 2 * margin;
        int maxCols = fullscreen_ ? 128 : 112;
        contentW = std::min(contentW, maxCols * cellW_);
        if (contentW < 20 * cellW_) contentW = std::max(cellW_ * 20, width - 2 * margin / 2);
        int contentX = (width - contentW) / 2;
        if (contentX < margin) contentX = margin;

        int topY = margin;
        int storyTop = topY + cellH_ + 14 * scale;
        int storyBottom = height - margin - cellH_ * 3 - 40 * scale;
        if (storyBottom < storyTop + cellH_ * 3) {
            storyBottom = storyTop + cellH_ * 3;
        }

        int scrollReserve = 16 * scale;
        int textW = std::max(20 * cellW_, contentW - scrollReserve);
        int cols = std::max(20, textW / cellW_);
        int rows = std::max(3, (storyBottom - storyTop) / cellH_);

        std::vector<VisualLine> lines = wrapBlocks(cols);
        totalLines_ = static_cast<int>(lines.size());
        visibleLines_ = rows;
        maxScroll_ = std::max(0, totalLines_ - rows);
        scroll_ = std::max(0, std::min(maxScroll_, scroll_));
        int startLine = std::max(0, totalLines_ - rows - scroll_);

        storyRect_ = {contentX - 6 * scale, storyTop - 4 * scale,
                      contentX + textW + 6 * scale, storyBottom + 4 * scale};

        // ---- header
        int y = topY;
        const std::wstring title = L"T H E   H O L L O W   S P I R E";
        drawText(dc, contentX, y, title, kInkAccent);
        SIZE titleSize = measure(dc, title);
        const std::wstring subtitle = L"/  an ASCII adventure";
        SIZE subtitleSize = measure(dc, subtitle);
        if (titleSize.cx + subtitleSize.cx + 20 * scale < contentW) {
            drawText(dc, contentX + titleSize.cx + 2 * cellW_, y, subtitle,
                     kInkDim);
        }
        std::wstring hint = fullscreen_
            ? L"F11 / ESC  LEAVE FULLSCREEN"
            : L"F1 HELP   F11 FULLSCREEN";
        SIZE hintSize = measure(dc, hint);
        int used = titleSize.cx + 2 * cellW_ + subtitleSize.cx;
        if (used + hintSize.cx + 40 * scale < contentW) {
            drawRight(dc, contentX + contentW, y, hint, kInkDim);
        }

        int ruleY = topY + cellH_ + 6 * scale;
        drawLine(dc, contentX, ruleY, contentX + contentW, ruleY, kBorder);
        drawLine(dc, contentX, ruleY, contentX + std::min(contentW, 200 * scale),
                 ruleY, kBorderBright);

        // ---- story
        int clip = SaveDC(dc);
        IntersectClipRect(dc, storyRect_.left, storyRect_.top, storyRect_.right,
                          storyRect_.bottom);
        y = storyTop;
        for (int row = 0; row < rows && startLine + row < totalLines_;
             ++row, y += cellH_) {
            const VisualLine& line = lines[startLine + row];
            int x = contentX;
            for (const auto& run : line.runs) {
                drawText(dc, x, y, run.text, inkColor(run.ink));
                x += static_cast<int>(run.text.size()) * cellW_;
            }
        }
        RestoreDC(dc, clip);

        // ---- scrollbar
        scrollTrackHit_ = {};
        scrollTrackHeight_ = 0;
        scrollThumbHeight_ = 0;
        storyTop_ = storyTop;
        if (totalLines_ > rows) {
            int trackX = contentX + contentW - 4 * scale;
            drawLine(dc, trackX, storyTop, trackX, storyBottom, kBorder);
            int trackHeight = storyBottom - storyTop;
            int thumbHeight = std::max(24 * scale,
                                       trackHeight * rows / std::max(1, totalLines_));
            int thumbTravel = std::max(1, trackHeight - thumbHeight);
            int thumbOffset = maxScroll_ > 0
                ? thumbTravel * startLine / maxScroll_
                : 0;
            RECT thumb{trackX - 1, storyTop + thumbOffset, trackX + 3 * scale,
                       storyTop + thumbOffset + thumbHeight};
            fillRectColor(dc, thumb, kInkArt);
            scrollTrackHit_ = {trackX - 8 * scale, storyTop,
                               trackX + 10 * scale, storyBottom};
            scrollTrackHeight_ = trackHeight;
            scrollThumbHeight_ = thumbHeight;
        }

        // ---- status
        int inputH = cellH_ + 14 * scale;
        int inputTop = height - margin - inputH;
        int statusY = inputTop - cellH_ - 8 * scale;
        int statusRuleY = statusY - 8 * scale;
        drawLine(dc, contentX, statusRuleY, contentX + contentW, statusRuleY, kBorder);

        std::wstring room = game_.gameOver() ? L"The End" : game_.location();
        drawText(dc, contentX, statusY, room,
                 game_.gameOver() ? kInkDim : kInkRoom);
        SIZE roomSize = measure(dc, room);
        std::wstring stats = L"  |  steps " + std::to_wstring(game_.moves()) +
                             L"  |  score " + std::to_wstring(game_.score()) +
                             L"  |  carrying " +
                             std::to_wstring(game_.carried());
        if (roomSize.cx + 20 * scale < contentW) {
            drawText(dc, contentX + roomSize.cx, statusY, stats, kInkDim);
        }
        std::wstring rightHint = flash_.empty()
            ? L"type HELP or press F1"
            : flash_;
        SIZE statsSize = measure(dc, stats);
        SIZE rightSize = measure(dc, rightHint);
        if (roomSize.cx + statsSize.cx + rightSize.cx + 60 * scale < contentW) {
            drawRight(dc, contentX + contentW, statusY, rightHint,
                      flash_.empty() ? kInkDim : kInkGood);
        }

        // ---- input line
        RECT inputRect{contentX, inputTop, contentX + contentW, inputTop + inputH};
        inputRect_ = inputRect;
        HBRUSH panelBrush = CreateSolidBrush(kPanel);
        HPEN inputPen = CreatePen(PS_SOLID, 1,
                                  focused_ ? kBorderBright : kBorder);
        HGDIOBJ oldBrush = SelectObject(dc, panelBrush);
        HGDIOBJ oldPen = SelectObject(dc, inputPen);
        RoundRect(dc, inputRect.left, inputRect.top, inputRect.right,
                  inputRect.bottom, 10 * scale, 10 * scale);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(panelBrush);
        DeleteObject(inputPen);

        int padX = 12 * scale;
        int promptWidth = 2 * cellW_;
        int textX = inputRect.left + padX + promptWidth;
        int textY = inputTop + (inputH - cellH_) / 2 + 2 * scale;
        inputTextX_ = textX;
        drawText(dc, inputRect.left + padX, textY, L">", kInkAccent);

        int availableForText = inputRect.right - padX - textX;
        int visibleChars = std::max(1, availableForText / cellW_);
        if (static_cast<int>(caret_) < inputViewStart_) {
            inputViewStart_ = static_cast<int>(caret_);
        }
        if (static_cast<int>(caret_) > inputViewStart_ + visibleChars - 1) {
            inputViewStart_ = static_cast<int>(caret_) - visibleChars + 1;
        }
        int maxView = std::max(0, static_cast<int>(input_.size()) - visibleChars);
        inputViewStart_ = std::max(0, std::min(inputViewStart_, maxView));

        if (input_.empty()) {
            if (!focused_) {
                drawText(dc, textX, textY, L"type a command... (help)", kInkDim);
            }
        } else {
            std::wstring visible =
                input_.substr(static_cast<size_t>(inputViewStart_),
                              static_cast<size_t>(visibleChars));
            drawText(dc, textX, textY, visible, kInkText);
        }

        if (focused_ && caretOn_) {
            int caretX = textX + (static_cast<int>(caret_) - inputViewStart_) * cellW_;
            RECT caretRect{caretX, textY + 1, caretX + std::max(1, 2 * scale),
                           textY + cellH_ - 2};
            if (caretRect.left < inputRect.right - padX) {
                fillRectColor(dc, caretRect, kInkAccent);
            }
        }

        SelectObject(dc, oldFont);
        BitBlt(screen, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }

    // --------------------------------------------------------------- members
    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HMENU menu_ = nullptr;

    Game game_;
    std::vector<Block> log_;
    std::wstring input_;
    size_t caret_ = 0;
    std::vector<std::wstring> history_;
    int historyPos_ = -1;
    std::wstring draft_;

    bool caretOn_ = true;
    bool focused_ = false;
    UINT dpi_ = 96;
    int fontPt_ = 12;
    HFONT font_ = nullptr;
    UINT fontDpi_ = 0;
    int fontPtCached_ = 0;

    int cellW_ = 8;
    int cellH_ = 18;
    int scroll_ = 0;
    int maxScroll_ = 0;
    int totalLines_ = 0;
    int visibleLines_ = 0;

    RECT storyRect_{};
    RECT inputRect_{};
    int storyTop_ = 0;
    RECT scrollTrackHit_{};
    int scrollTrackHeight_ = 0;
    int scrollThumbHeight_ = 0;
    bool draggingScroll_ = false;
    int inputTextX_ = 0;
    int inputViewStart_ = 0;

    bool tabActive_ = false;
    size_t tabStart_ = 0;
    std::wstring tabPrefix_;
    std::vector<std::wstring> tabCandidates_;
    size_t tabIndex_ = 0;

    std::wstring flash_;
    ULONGLONG flashUntil_ = 0;

    bool fullscreen_ = false;
    WINDOWPLACEMENT placement_{};
    LONG windowedStyle_ = 0;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    enableDpiAwareness();

    App app;
    if (!app.create(instance)) {
        MessageBoxW(nullptr, L"The window could not be created.",
                    L"The Hollow Spire", MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
