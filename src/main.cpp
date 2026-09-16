#include <windows.h>
#include <atomic>
#include "updater.h"

static HINSTANCE g_hInstance = nullptr;
static HANDLE g_hThread = nullptr;
static std::atomic<bool> g_has_run{false};

static DWORD WINAPI PatcherThread(LPVOID lpParam) {
    try {
        PatchUpdater updater(g_hInstance);
        updater.run();
    } catch (...) {
        // Fail-safe
    }
    return 0;
}

extern "C" __declspec(dllexport) void __cdecl InitPatches() {
    if (g_has_run.exchange(true)) return;

    if (!g_hThread) {
        PatcherThread(nullptr);
    } else {
        WaitForSingleObject(g_hThread, INFINITE);
    }
}

extern "C" __declspec(dllexport) void CALLBACK CheckPatches(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
    if (g_has_run.exchange(true)) {
        if (g_hThread) {
            WaitForSingleObject(g_hThread, INFINITE);
        }
        return;
    }
    PatcherThread(nullptr);
}

// Optional compatibility stubs if patched into Wow.exe IAT
extern "C" __declspec(dllexport) int __cdecl DivxDecode() { return 0; }
extern "C" __declspec(dllexport) int __cdecl InitializeDivxDecoder() { return 0; }
extern "C" __declspec(dllexport) int __cdecl SetOutputFormat() { return 0; }
extern "C" __declspec(dllexport) int __cdecl UnInitializeDivxDecoder() { return 0; }

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH: {
            g_hInstance = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);

            // Spawn worker thread so networking and UI NEVER run under the loader lock!
            if (!g_has_run.exchange(true)) {
                g_hThread = CreateThread(nullptr, 0, PatcherThread, nullptr, 0, nullptr);
            }
            break;
        }
        case DLL_PROCESS_DETACH:
            if (g_hThread) {
                CloseHandle(g_hThread);
                g_hThread = nullptr;
            }
            break;
    }
    return TRUE;
}
