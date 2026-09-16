<#
.SYNOPSIS
    Integrates bpatches.dll directly into Wow.exe so double-clicking Wow.exe automatically runs bpatches.

.DESCRIPTION
    Replaces the import string 'DivxDecoder.dll' with 'bpatches.dll\0\0\0\0' in Wow.exe's import directory.
    Creates a backup 'Wow.exe.bak' before making any changes.
    Can be reverted at any time with -Revert.

.PARAMETER WowExePath
    Path to Wow.exe (auto-detects .\Wow.exe or ..\Wow.exe if omitted).

.PARAMETER Revert
    Restores the original Wow.exe from Wow.exe.bak.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$WowExePath = "",

    [Parameter(Mandatory = $false)]
    [switch]$Revert
)

$ErrorActionPreference = "Stop"

if (-not $WowExePath) {
    if (Test-Path ".\Wow.exe") {
        $WowExePath = ".\Wow.exe"
    } elseif (Test-Path "..\Wow.exe") {
        $WowExePath = "..\Wow.exe"
    } else {
        Write-Error "Could not find Wow.exe in current or parent directory."
        return
    }
}

# Check if Wow.exe is currently running
$running = Get-Process Wow -ErrorAction SilentlyContinue
if ($running) {
    Write-Warning "Wow.exe is currently running. Please close World of Warcraft before running this script."
    return
}

$wowFull = (Get-Item $WowExePath).FullName
$bakPath = "$wowFull.bak"

if ($Revert) {
    if (-not (Test-Path $bakPath)) {
        Write-Error "Backup file $bakPath not found. Cannot revert."
        return
    }
    Copy-Item $bakPath -Destination $wowFull -Force
    Write-Host "Reverted $wowFull from backup!" -ForegroundColor Green
    return
}

# Create backup if not exists
if (-not (Test-Path $bakPath)) {
    Copy-Item $wowFull -Destination $bakPath
    Write-Host "Created backup: $bakPath" -ForegroundColor Cyan
}

$bytes = [System.IO.File]::ReadAllBytes($wowFull)
# Target is the import directory name specifically following InitializeDivxDecoder
$targetPattern = [System.Text.Encoding]::ASCII.GetBytes("InitializeDivxDecoder`0DivxDecoder.dll`0")
$replacement   = [System.Text.Encoding]::ASCII.GetBytes("InitializeDivxDecoder`0bpatches.dll`0`0`0`0")

$found = $false
for ($i = 0; $i -le ($bytes.Length - $targetPattern.Length); $i++) {
    $match = $true
    for ($j = 0; $j -lt $targetPattern.Length; $j++) {
        if ($bytes[$i + $j] -ne $targetPattern[$j]) {
            $match = $false
            break
        }
    }
    if ($match) {
        for ($j = 0; $j -lt $targetPattern.Length; $j++) {
            $bytes[$i + $j] = $replacement[$j]
        }
        $found = $true
        Write-Host "Found and patched import directory entry at offset 0x$($i.ToString('X8'))" -ForegroundColor Green
        break
    }
}

if (-not $found) {
    # Check if already patched
    $alreadyPatchedPattern = [System.Text.Encoding]::ASCII.GetBytes("InitializeDivxDecoder`0bpatches.dll`0`0`0`0")
    $already = $false
    for ($i = 0; $i -le ($bytes.Length - $alreadyPatchedPattern.Length); $i++) {
        $match = $true
        for ($j = 0; $j -lt $alreadyPatchedPattern.Length; $j++) {
            if ($bytes[$i + $j] -ne $alreadyPatchedPattern[$j]) {
                $match = $false
                break
            }
        }
        if ($match) {
            $already = $true
            break
        }
    }
    if ($already) {
        Write-Host "Wow.exe is ALREADY patched with bpatches.dll!" -ForegroundColor Yellow
        return
    } else {
        Write-Error "Could not locate 'InitializeDivxDecoder\0DivxDecoder.dll\0' in Wow.exe."
        return
    }
}

[System.IO.File]::WriteAllBytes($wowFull, $bytes)
Write-Host "Successfully patched Wow.exe! Double-clicking Wow.exe will now automatically load bpatches.dll." -ForegroundColor Green
