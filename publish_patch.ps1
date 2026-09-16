<#
.SYNOPSIS
    Publishes custom patches to GitHub Releases for 11011010/bpatches.

.DESCRIPTION
    Packs custom MPQ patches into .zip or .rar, computes hashes, generates manifest.json,
    and publishes a GitHub Release with the archive assets attached.

.PARAMETER Tag
    The release tag (e.g. "v1.0.0").

.PARAMETER Title
    Release title (defaults to Tag).

.PARAMETER Notes
    Release notes or description.

.PARAMETER PatchFiles
    One or more paths to .mpq files.

.PARAMETER AsZip
    Automatically packages each MPQ into a .zip file (e.g. patch-5.mpq -> patch-5.zip) before uploading.

.EXAMPLE
    .\publish_patch.ps1 -Tag "v1.0.0" -PatchFiles "Data\patch-5.mpq" -AsZip -Notes "Balance patch 5"
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Tag,

    [Parameter(Mandatory = $false)]
    [string]$Title = $Tag,

    [Parameter(Mandatory = $false)]
    [string]$Notes = "Custom patches update for BalanceWoW ChromieCraft 3.3.5a",

    [Parameter(Mandatory = $true)]
    [string[]]$PatchFiles,

    [Parameter(Mandatory = $false)]
    [switch]$AsZip,

    [Parameter(Mandatory = $false)]
    [string]$Repo = "11011010/bpatches"
)

$ErrorActionPreference = "Stop"

Write-Host "=== BalanceWoW Patch Publisher ===" -ForegroundColor Cyan
Write-Host "Repository : $Repo"
Write-Host "Tag        : $Tag"
Write-Host "Title      : $Title"

$verifiedFiles = @()
$manifestPatches = @()

foreach ($filePath in $PatchFiles) {
    if (-not (Test-Path $filePath)) {
        Write-Error "Patch file not found: $filePath"
        return
    }

    $item = Get-Item $filePath
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($item.Name)

    # Determine relative path in client (e.g., Data/patch-5.mpq or Data/deDE/patch-deDEc.mpq)
    $relPath = "Data/$($item.Name)"
    $locales = @("deDE", "enUS", "enGB", "frFR", "ruRU", "esES", "zhCN", "zhTW", "koKR")
    foreach ($loc in $locales) {
        if ($item.FullName -match "\\Data\\$loc\\") {
            $relPath = "Data/$loc/$($item.Name)"
            break
        }
    }

    $uploadFile = $item.FullName

    if ($AsZip -and $item.Extension -eq ".mpq") {
        $zipName = "$baseName.zip"
        $zipPath = Join-Path $PSScriptRoot $zipName
        Write-Host "Packing $($item.Name) into $zipName..." -ForegroundColor Yellow
        if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
        Compress-Archive -Path $item.FullName -DestinationPath $zipPath -Force
        $uploadFile = $zipPath
        $uploadItem = Get-Item $uploadFile
        Write-Host "Packed: $($uploadItem.Length) bytes." -ForegroundColor Green
    }

    $uploadItem = Get-Item $uploadFile
    $hash = (Get-FileHash -Path $uploadFile -Algorithm SHA256).Hash.ToLower()

    $manifestPatches += [PSCustomObject]@{
        name     = $uploadItem.Name
        rel_path = $relPath
        size     = $uploadItem.Length
        sha256   = $hash
    }

    $verifiedFiles += $uploadFile
}

# Create manifest.json
$manifestObj = [PSCustomObject]@{
    version  = $Tag
    created  = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    patches  = $manifestPatches
}

$manifestPath = Join-Path $PSScriptRoot "manifest.json"
$manifestJson = $manifestObj | ConvertTo-Json -Depth 5
Set-Content -Path $manifestPath -Value $manifestJson -Encoding utf8
Write-Host "Generated manifest.json successfully." -ForegroundColor Green
$verifiedFiles += $manifestPath

# Check if gh CLI is available
$hasGh = Get-Command gh -ErrorAction SilentlyContinue
if ($hasGh) {
    Write-Host "Creating GitHub Release via gh CLI..." -ForegroundColor Yellow
    $ghArgs = @("release", "create", $Tag, "--repo", $Repo, "--title", $Title, "--notes", $Notes) + $verifiedFiles
    & gh @ghArgs
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Release $Tag successfully published to GitHub!" -ForegroundColor Green
        return
    } else {
        Write-Warning "gh CLI command exited with code $LASTEXITCODE. Trying git tag fallback..."
    }
}

# Fallback: Git tag push
Write-Host "Creating and pushing git tag $Tag..." -ForegroundColor Yellow
git tag -a $Tag -m "$Title`n`n$Notes"
git push origin $Tag
Write-Host "Git tag pushed. You can now drag and drop the files into GitHub Release: https://github.com/$Repo/releases" -ForegroundColor Cyan
