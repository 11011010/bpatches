#include <windows.h>
#include <string>

typedef void (__cdecl *InitPatchesFn)();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Load bpatches.dll and execute the patch check
    HMODULE hPatcher = LoadLibraryA("bpatches.dll");
    if (hPatcher) {
        InitPatchesFn initPatches = (InitPatchesFn)GetProcAddress(hPatcher, "InitPatches");
        if (initPatches) {
            initPatches();
        }
        FreeLibrary(hPatcher);
    }

    // 2. Launch Wow.exe with any passed arguments
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    std::string cmd = "Wow.exe";
    if (lpCmdLine && strlen(lpCmdLine) > 0) {
        cmd += " ";
        cmd += lpCmdLine;
    }

    char cmd_buf[1024];
    strncpy_s(cmd_buf, cmd.c_str(), sizeof(cmd_buf) - 1);

    if (CreateProcessA(nullptr, cmd_buf, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        MessageBoxA(nullptr, "Failed to start Wow.exe. Please ensure Wow.exe is in the same directory.", "BalanceWoW Launcher Error", MB_ICONERROR);
    }

    return 0;
}
