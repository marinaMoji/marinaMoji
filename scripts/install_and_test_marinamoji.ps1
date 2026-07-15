# Install and test marinaMoji IME on Windows
# Prerequisites:
#   - marinaMoji64.msi exists at specified path
#   - Stock Mozc is already installed (for side-by-side testing)
#   - Admin privileges required
# Usage: powershell -ExecutionPolicy Bypass -File install_and_test_marinamoji.ps1 -MsiPath C:\path\to\marinaMoji64.msi

param(
    [Parameter(Mandatory=$true)]
    [string]$MsiPath,
    [switch]$NoInstall = $false,
    [switch]$UninstallAfter = $false
)

$ErrorActionPreference = "Stop"

# Helper: Test admin privileges
function Test-AdminPrivilege {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Helper: Run command and capture output
function Invoke-Cmd {
    param([string]$Cmd, [string]$Args, [string]$Description)
    Write-Host "  → $Description" -ForegroundColor Gray
    try {
        if ($Args) {
            & $Cmd $Args.Split(' ') 2>&1 | Out-String | Write-Host -ForegroundColor DarkGray
        } else {
            & $Cmd 2>&1 | Out-String | Write-Host -ForegroundColor DarkGray
        }
        return $true
    } catch {
        Write-Host "    ✗ Failed: $_" -ForegroundColor Red
        return $false
    }
}

Write-Host "=== marinaMoji Installation & Acceptance Test ===" -ForegroundColor Cyan
Write-Host ""

# Check admin
if (-not (Test-AdminPrivilege)) {
    Write-Host "✗ This script requires administrator privileges" -ForegroundColor Red
    Write-Host "  Rerun: powershell -ExecutionPolicy Bypass -Command `"Start-Process powershell -ArgumentList '-File $PSCommandPath' -Verb RunAs`"" -ForegroundColor Yellow
    exit 1
}

# Verify MSI
if (-not (Test-Path $MsiPath)) {
    Write-Host "✗ MSI not found: $MsiPath" -ForegroundColor Red
    exit 1
}

$msiSize = (Get-Item $MsiPath).Length / 1MB
Write-Host "MSI: $MsiPath" -ForegroundColor Green
Write-Host "Size: $([Math]::Round($msiSize, 2)) MB" -ForegroundColor Gray
Write-Host ""

# Test 1: Verify stock Mozc is installed
Write-Host "Test 1: Verify stock Mozc is installed" -ForegroundColor Yellow
$mozcKey = "HKLM:\SOFTWARE\Mozc Project\Mozc"
if (Test-Path $mozcKey) {
    Write-Host "  ✓ Stock Mozc registry key found" -ForegroundColor Green
} else {
    Write-Host "  ⚠ Stock Mozc registry key not found (side-by-side test may not be valid)" -ForegroundColor Yellow
}
Write-Host ""

# Test 2: Install marinaMoji
if (-not $NoInstall) {
    Write-Host "Test 2: Install marinaMoji64.msi" -ForegroundColor Yellow
    Write-Host "  → Running msiexec..." -ForegroundColor Gray
    $installArgs = "/i `"$MsiPath`" /quiet /norestart"
    Start-Process -FilePath "msiexec.exe" -ArgumentList $installArgs -NoNewWindow -Wait
    Start-Sleep -Seconds 2

    # Check registry
    $marinamojiKey = "HKLM:\SOFTWARE\CRCAO\marinaMoji"
    if (Test-Path $marinamojiKey) {
        Write-Host "  ✓ marinaMoji installed (registry key found)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Installation may have failed (registry key not found)" -ForegroundColor Red
        exit 1
    }
    Write-Host ""
}

# Test 3: Verify profile directory
Write-Host "Test 3: Verify profile directory" -ForegroundColor Yellow
$profileDir = "$env:LOCALAPPDATA\marinaMoji"
if (Test-Path $profileDir) {
    Write-Host "  ✓ Profile dir exists: $profileDir" -ForegroundColor Green
    $files = Get-ChildItem $profileDir -Recurse 2>/dev/null | Measure-Object
    Write-Host "    Contains $($files.Count) files/dirs" -ForegroundColor Gray
} else {
    Write-Host "  ⚠ Profile dir not yet created (may be created on first use)" -ForegroundColor Yellow
}
Write-Host ""

# Test 4: Check that both IMEs appear in Windows input settings
Write-Host "Test 4: Windows input settings (registry check)" -ForegroundColor Yellow
$imeTipKey = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\AutoplayHandlers\Handlers\DirectShowPlugins"
$tzPath = "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layouts"

Write-Host "  → Checking installed TIP CLSIDs..." -ForegroundColor Gray
$tzKey = Get-Item -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Keyboard Layouts" -ErrorAction SilentlyContinue
if ($tzKey) {
    $layouts = $tzKey.SubKeyNames
    $mozcFound = $layouts | Where-Object { $_ -like "*10A67BC8*" }
    $marinamojiFound = $layouts | Where-Object { $_ -like "*8D513EAE*" }

    if ($mozcFound) { Write-Host "    ✓ Mozc TIP CLSID found" -ForegroundColor Green }
    if ($marinamojiFound) { Write-Host "    ✓ marinaMoji TIP CLSID found" -ForegroundColor Green }
    if (-not $mozcFound -or -not $marinamojiFound) {
        Write-Host "    ⚠ Check Settings → Time & Language → Language → Japanese → Keyboard manually" -ForegroundColor Yellow
    }
}
Write-Host ""

# Test 5: Verify binaries exist
Write-Host "Test 5: Check marinaMoji binaries" -ForegroundColor Yellow
$programFiles = "C:\Program Files (x86)"
$binariesNeeded = @(
    "marinaMoji\marinamoji_server.exe",
    "marinaMoji\marinamoji_tip64.dll",
    "marinaMoji\marinamoji_renderer.exe",
    "marinaMoji\marinamoji_tool.exe",
    "marinaMoji\marinaMojiCacheService.exe"
)

$allFound = $true
foreach ($bin in $binariesNeeded) {
    $path = Join-Path $programFiles $bin
    if (Test-Path $path) {
        Write-Host "  ✓ $bin" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $bin not found" -ForegroundColor Red
        $allFound = $false
    }
}

if (-not $allFound) {
    Write-Host "  ⚠ Some binaries missing — installation may have failed" -ForegroundColor Yellow
}
Write-Host ""

# Test 6: Verify marinaMoji cache service is registered
Write-Host "Test 6: Check Windows service registration" -ForegroundColor Yellow
$serviceKey = "HKLM:\SYSTEM\CurrentControlSet\Services\marinaMojiCacheService"
if (Test-Path $serviceKey) {
    Write-Host "  ✓ marinaMojiCacheService registered" -ForegroundColor Green
} else {
    Write-Host "  ✗ marinaMojiCacheService not registered" -ForegroundColor Red
}
Write-Host ""

# Test 7: Verify pipes/mutexes don't collide
Write-Host "Test 7: Check IPC naming isolation" -ForegroundColor Yellow
Write-Host "  → marinaMoji should use \\.\\pipe\\marinamoji.*" -ForegroundColor Gray
Write-Host "  → Stock Mozc should use \\.\\pipe\\googlejapaneseinput.* or \\.\\pipe\\mozc.*" -ForegroundColor Gray
Write-Host "  ⓘ This is verified at runtime when both IMEs are active" -ForegroundColor Gray
Write-Host "    (no static way to check; manual test in Phase 1g)" -ForegroundColor Gray
Write-Host ""

# Test 8: Summary
Write-Host "Test 8: Acceptance checklist" -ForegroundColor Yellow
Write-Host "  Manual tests (requires Notepad or text editor):" -ForegroundColor Gray
Write-Host "    1. Alt+` to open IME menu → marinaMoji should appear" -ForegroundColor Gray
Write-Host "    2. Switch to marinaMoji and type 'a' → ぁ candidates should appear" -ForegroundColor Gray
Write-Host "    3. Type 'kanji' → 漢字 candidates should appear" -ForegroundColor Gray
Write-Host "    4. Open marinaMoji Preferences (right-click IME → Settings)" -ForegroundColor Gray
Write-Host "    5. Verify stock Mozc still works (switch to Mozc, test same input)" -ForegroundColor Gray
Write-Host "    6. Test marinaMoji Preferences (Qt tool should launch)" -ForegroundColor Gray
Write-Host ""

# Test 9: Uninstall if requested
if ($UninstallAfter) {
    Write-Host "Test 9: Uninstall marinaMoji" -ForegroundColor Yellow
    Write-Host "  → Running uninstall..." -ForegroundColor Gray
    Start-Process -FilePath "msiexec.exe" -ArgumentList "/x `"$MsiPath`" /quiet /norestart" -NoNewWindow -Wait
    Start-Sleep -Seconds 2

    $marinamojiKey = "HKLM:\SOFTWARE\CRCAO\marinaMoji"
    if (-not (Test-Path $marinamojiKey)) {
        Write-Host "  ✓ Uninstall successful" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Uninstall may have failed (registry key still present)" -ForegroundColor Red
    }
    Write-Host ""
}

Write-Host "=== Phase 1g Acceptance Test Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  - If all checks passed, marinaMoji is ready for Phase 2 (session features)" -ForegroundColor Gray
Write-Host "  - If any check failed, debug output is logged above" -ForegroundColor Gray
