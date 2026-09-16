<#
.SYNOPSIS
    Integrates bpatches.dll directly into Wow.exe so double-clicking Wow.exe automatically runs bpatches.

.DESCRIPTION
    Replaces the import string 'DivxDecoder.dll' with 'bpatches.dll\0\0\0' in Wow.exe.
    Creates a backup 'Wow.exe.bak' before making any changes.
    Can be reverted at any time with -Revert.

.PARAMETER WowExePath
    Path to Wow.exe (defaults to ..\Wow.exe).

.PARAMETER Revert
    Restores the original Wow.exe from Wow.exe.bak.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$WowExePath = "..\Wow.exe",

    [Parameter(Mandatory = $false)]
    [switch]$Revert
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $WowExePath)) {
    Write-Error "Wow.exe not found at $WowExePath"
    return
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
$origStr = [System.Text.Encoding]::ASCII.GetBytes("DivxDecoder.dll`0")
$newStr  = [System.Text.Encoding]::ASCII.GetBytes("bpatches.dll`0`0`0`0")

$found = $false
for ($i = 0; $i -le ($bytes.Length - $origStr.Length); $i++) {
    $match = $true
    for ($j = 0; $j -lt $origStr.Length; $j++) {
        if ($bytes[$i + $j] -ne $origStr[$j]) {
            $match = $false
            break
        }
    }
    if ($match) {
        # Replace
        for ($j = 0; $j -lt $origStr.Length; $j++) {
            $bytes[$i + $j] = $newStr[$j]
        }
        $found = $true
        Write-Host "Found and patched import entry at offset 0x$($i.ToString('X8'))" -ForegroundColor Green
        break
    }
}

if (-not $found) {
    # Check if already patched
    $alreadyPatched = $false
    for ($i = 0; $i -le ($bytes.Length - $newStr.Length); $i++) {
        $match = $true
        for ($j = 0; $j -lt $newStr.Length; $j++) {
            if ($bytes[$i + $j] -ne $newStr[$j]) {
                $match = $false
                break
            }
        }
        if ($match) {
            $alreadyPatched = $true
            break
        }
    }
    if ($alreadyPatched) {
        Write-Host "Wow.exe is ALREADY patched with bpatches.dll!" -ForegroundColor Yellow
        return
    } else {
        Write-Error "Could not locate 'DivxDecoder.dll' import string in Wow.exe."
        return
    }
}

[System.IO.File]::WriteAllBytes($wowFull, $bytes)
Write-Host "Successfully patched Wow.exe! Double-clicking Wow.exe will now automatically load bpatches.dll." -ForegroundColor Green
