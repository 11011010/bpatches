#include "ui.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

static const char* kClassName = "BalanceWoWPatchUpdaterWnd";

PatchUI::PatchUI() {}

PatchUI::~PatchUI() {
    close();
}

PatchUI* PatchUI::get() {
    static PatchUI instance;
    return &instance;
}

LRESULT CALLBACK PatchUI::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLOSE || msg == WM_DESTROY) {
        PatchUI::get()->closed_ = true;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

bool PatchUI::init(HINSTANCE hInstance, const std::string& title) {
    if (hwnd_) return true;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = PatchUI::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExA(&wc);

    int width = 460;
    int height = 150;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_w - width) / 2;
    int y = (screen_h - height) / 2;

    hwnd_ = CreateWindowExA(
        WS_EX_TOPMOST,
        kClassName,
        title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd_) return false;

    NONCLIENTMETRICSA ncm = { sizeof(NONCLIENTMETRICSA) };
    SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSA), &ncm, 0);

    LOGFONTA lf_title = ncm.lfMessageFont;
    lf_title.lfWeight = FW_BOLD;
    lf_title.lfHeight = -14;
    hfont_title_ = CreateFontIndirectA(&lf_title);

    LOGFONTA lf_text = ncm.lfMessageFont;
    lf_text.lfHeight = -12;
    hfont_text_ = CreateFontIndirectA(&lf_text);

    hwnd_title_ = CreateWindowExA(
        0, "STATIC", "Checking custom patches...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 15, width - 40, 20,
        hwnd_, nullptr, hInstance, nullptr
    );
    SendMessageA(hwnd_title_, WM_SETFONT, (WPARAM)hfont_title_, TRUE);

    hwnd_status_ = CreateWindowExA(
        0, "STATIC", "Connecting to GitHub...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 42, width - 40, 20,
        hwnd_, nullptr, hInstance, nullptr
    );
    SendMessageA(hwnd_status_, WM_SETFONT, (WPARAM)hfont_text_, TRUE);

    hwnd_progress_ = CreateWindowExA(
        0, PROGRESS_CLASSA, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        20, 72, width - 40, 22,
        hwnd_, nullptr, hInstance, nullptr
    );

    SendMessageA(hwnd_progress_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageA(hwnd_progress_, PBM_SETPOS, 0, 0);

    UpdateWindow(hwnd_);
    process_events();
    return true;
}

void PatchUI::show() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        process_events();
    }
}

void PatchUI::hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
        process_events();
    }
}

void PatchUI::close() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (hfont_title_) {
        DeleteObject(hfont_title_);
        hfont_title_ = nullptr;
    }
    if (hfont_text_) {
        DeleteObject(hfont_text_);
        hfont_text_ = nullptr;
    }
}

void PatchUI::set_status(const std::string& title_text, const std::string& detail_text) {
    if (hwnd_title_) {
        SetWindowTextA(hwnd_title_, title_text.c_str());
    }
    if (hwnd_status_) {
        SetWindowTextA(hwnd_status_, detail_text.c_str());
    }
    process_events();
}

void PatchUI::set_progress(int percent) {
    if (hwnd_progress_) {
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        SendMessageA(hwnd_progress_, PBM_SETPOS, percent, 0);
    }
    process_events();
}

void PatchUI::process_events() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}
