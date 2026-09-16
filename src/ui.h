#pragma once

#include <string>
#include <cstdint>
#include <windows.h>

class PatchUI {
public:
    static PatchUI* get();

    bool init(HINSTANCE hInstance, const std::string& title = "BalanceWoW Patch Updater");
    void show();
    void hide();
    void close();

    void set_status(const std::string& title_text, const std::string& detail_text);
    void set_progress(int percent); // 0 to 100, or -1 for marquee/indeterminate
    void process_events();

    bool is_closed() const { return closed_; }

private:
    PatchUI();
    ~PatchUI();

    HWND hwnd_ = nullptr;
    HWND hwnd_title_ = nullptr;
    HWND hwnd_status_ = nullptr;
    HWND hwnd_progress_ = nullptr;
    HFONT hfont_title_ = nullptr;
    HFONT hfont_text_ = nullptr;
    bool closed_ = false;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
