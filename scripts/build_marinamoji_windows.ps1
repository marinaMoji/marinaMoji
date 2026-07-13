# Build marinaMoji on Windows (x64)
# Prerequisites: VS 2022, Python 3.12+, .NET 6+, Bazelisk in PATH
# Usage: powershell -ExecutionPolicy Bypass -File build_marinamoji_windows.ps1

param(
    [string]$RepoRoot = (git rev-parse --show-toplevel),
    [switch]$SkipDeps = $false,
    [switch]$SkipQt = $false,
    [switch]$CleanBuild = $false
)

$ErrorActionPreference = "Stop"

Write-Host "=== marinaMoji Windows Build ===" -ForegroundColor Cyan
Write-Host "Repo: $RepoRoot" -ForegroundColor Gray
Write-Host ""

# Verify prerequisites
Write-Host "Checking prerequisites..." -ForegroundColor Yellow
$prereqs = @{
    "python" = "Python 3.12+"
    "bazelisk" = "Bazelisk"
    "dotnet" = ".NET 6+"
}

foreach ($cmd in $prereqs.Keys) {
    try {
        $result = & $cmd --version 2>&1
        Write-Host "  ✓ $($prereqs[$cmd])" -ForegroundColor Green
    } catch {
        Write-Host "  ✗ $($prereqs[$cmd]) not found in PATH" -ForegroundColor Red
        exit 1
    }
}
Write-Host ""

# Change to src directory
Push-Location "$RepoRoot\src"
$srcDir = Get-Location

try {
    # Step 1: Update dependencies
    if (-not $SkipDeps) {
        Write-Host "Step 1: Downloading build dependencies..." -ForegroundColor Yellow
        Write-Host "  (LLVM, MSYS2, Qt, Ninja, .NET tools)" -ForegroundColor Gray
        & python build_tools/update_deps.py
        if ($LASTEXITCODE -ne 0) {
            throw "update_deps.py failed with exit code $LASTEXITCODE"
        }
        Write-Host "  ✓ Dependencies downloaded" -ForegroundColor Green
        Write-Host ""
    }

    # Step 2: Build Qt
    if (-not $SkipQt) {
        Write-Host "Step 2: Building Qt 6 (may take 15-30 min)..." -ForegroundColor Yellow
        & python build_tools/build_qt.py --release --confirm_license
        if ($LASTEXITCODE -ne 0) {
            throw "build_qt.py failed with exit code $LASTEXITCODE"
        }
        Write-Host "  ✓ Qt 6 built" -ForegroundColor Green
        Write-Host ""
    }

    # Step 3: Clean Bazel cache if requested
    if ($CleanBuild) {
        Write-Host "Step 3: Cleaning Bazel cache..." -ForegroundColor Yellow
        & bazelisk clean --expunge
        Write-Host "  ✓ Bazel cache cleaned" -ForegroundColor Green
        Write-Host ""
    }

    # Step 4: Build marinaMoji package
    Write-Host "Step 4: Building marinaMoji (x64)..." -ForegroundColor Yellow
    Write-Host "  Config: oss_windows release_build" -ForegroundColor Gray
    Write-Host "  Target: marinaMoji64.msi" -ForegroundColor Gray
    & bazelisk build --config oss_windows --config release_build package
    if ($LASTEXITCODE -ne 0) {
        throw "Bazel build failed with exit code $LASTEXITCODE"
    }
    Write-Host "  ✓ Build succeeded" -ForegroundColor Green
    Write-Host ""

    # Step 5: Verify output
    $msiPath = "$srcDir\bazel-bin\win32\installer\marinaMoji64.msi"
    if (Test-Path $msiPath) {
        $msiSize = (Get-Item $msiPath).Length / 1MB
        Write-Host "Step 5: Output ready" -ForegroundColor Yellow
        Write-Host "  MSI: $msiPath" -ForegroundColor Green
        Write-Host "  Size: $([Math]::Round($msiSize, 2)) MB" -ForegroundColor Gray
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "  1. Copy marinaMoji64.msi to the test machine" -ForegroundColor Gray
        Write-Host "  2. Ensure stock Mozc is already installed on test machine" -ForegroundColor Gray
        Write-Host "  3. Run: powershell -File install_and_test_marinamoji.ps1 -MsiPath <path>" -ForegroundColor Gray
    } else {
        throw "Expected output not found: $msiPath"
    }

} finally {
    Pop-Location
}

Write-Host ""
Write-Host "=== Build complete ===" -ForegroundColor Cyan
