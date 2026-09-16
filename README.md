# bpatches

Lightweight 32-bit (x86) custom patch updater DLL for Balance WoW, integrated directly with GitHub Releases.

## Overview

`bpatches.dll` checks on launch whether custom patches (`.mpq` assets attached to GitHub Releases on `11011010/bpatches`) are installed and up to date.
If any patch is missing or outdated, it displays a native progress dialog, downloads the patch, verifies its integrity, and moves it to the appropriate client folder (`Data/` or `Data/<locale>/`).

## Features

- **GitHub Releases Integration**: Directly uses the GitHub Releases section of [`11011010/bpatches`](https://github.com/11011010/bpatches).
- **Intelligent Routing**:
  - General patches (e.g. `patch-5.mpq`, `patch-W.mpq`) are placed into `Data/`.
  - Locale patches (e.g. `patch-enUS-4.mpq`, `patch-enUSc.mpq`) are placed into `Data/enUS/` (or matching locale folder).
- **Zero Heavy Dependencies**: Built with native Win32/WinINet and static MSVC runtime (`/MT`). No extra runtimes needed.
- **Fail-Safe Startup**: If offline or GitHub is unreachable, it logs a warning and lets the game continue launching without freezing or crashing.
- **Progress Dialog**: Smooth native Windows progress bar showing download percentage and speed/size.
- **Single-Flight Safe**: Safe against duplicate launches and runs outside the Windows loader lock.

---

## How to Load `bpatches.dll`

You can use either of the following methods to load `bpatches.dll`:

### Method 1: `BalanceWoW.exe` (Recommended)
Included in the build is a tiny (30KB) launcher `BalanceWoW.exe`.
- Place `BalanceWoW.exe`, `bpatches.dll`, and `bpatches.ini` in your WoW directory.
- Double-click `BalanceWoW.exe`. It runs the patch check and then launches `Wow.exe`.

### Method 2: Direct `Wow.exe` Integration (`add_to_wow.ps1`)
If you want to double-click `Wow.exe` directly without any launcher:
1. Close World of Warcraft.
2. Run PowerShell as user in the `bpatches` folder:
   ```powershell
   .\add_to_wow.ps1
   ```
   This safely backs up `Wow.exe` to `Wow.exe.bak` and adds `bpatches.dll` to `Wow.exe`'s import table.
3. To revert back anytime:
   ```powershell
   .\add_to_wow.ps1 -Revert
   ```

### Method 3: Manual Check / Third-Party Injector
You can run the patch check outside the game at any time:
```cmd
C:\Windows\SysWOW64\rundll32.exe bpatches.dll,CheckPatches
```
Or export `InitPatches()` via your own custom launcher or injector.

---

## Configuration (`bpatches.ini`)

Place `bpatches.ini` next to `bpatches.dll` in your WoW root folder:

```ini
[bpatches]
; GitHub repository
Repo=11011010/bpatches

; Network timeout in seconds for GitHub API query
TimeoutSeconds=5

; Show visual progress dialog during patch checking and downloading (1 = enabled, 0 = disabled)
ShowUI=1

; Only process .mpq / .MPQ assets from releases (1 = enabled, 0 = all assets)
OnlyMpq=1

; Optional custom API endpoint (leave blank for standard GitHub releases API)
ApiUrl=
```

---

## How to Publish Custom Patches

### Option A: Using the PowerShell Publisher Script (`publish_patch.ps1`)
```powershell
.\publish_patch.ps1 -Tag "v1.0.0" -PatchFiles "Data\patch-5.mpq" -Notes "Custom balance updates and new items"
```
This script:
1. Calculates the SHA256 checksum for all specified `.mpq` files.
2. Generates `manifest.json`.
3. Creates a new GitHub Release and uploads the `.mpq` files + `manifest.json`.

### Option B: Via GitHub Web Interface
1. Go to [https://github.com/11011010/bpatches/releases/new](https://github.com/11011010/bpatches/releases/new).
2. Choose a tag (e.g. `v1.0.0`).
3. Drag & drop your `.mpq` files into the release assets section.
4. Click **Publish release**.

`bpatches.dll` will automatically detect and download the new patch on next game launch!

---

## Building from Source

Requirements:
- Visual Studio 2022 with C++ Desktop Development tools
- CMake 3.16+

```cmd
cmake -B build -A Win32
cmake --build build --config Release
```
Output files will be located in `build/Release/`:
- `bpatches.dll`
- `BalanceWoW.exe`
