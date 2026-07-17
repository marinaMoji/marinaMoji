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

# Resolve bazelisk: prefer PATH, fall back to the repo-root bazelisk.exe.
$bazelisk = $null
$bazeliskCmd = Get-Command bazelisk -ErrorAction SilentlyContinue
if ($bazeliskCmd) {
    $bazelisk = "bazelisk"
} elseif (Test-Path "$RepoRoot\bazelisk.exe") {
    $bazelisk = "$RepoRoot\bazelisk.exe"
}

# Resolve a real python interpreter, skipping the Microsoft Store
# "app execution alias" stub that Windows puts on PATH ahead of any real
# install (it writes to stderr, which -- combined with $ErrorActionPreference
# = Stop below -- turns into a terminating error even when a real Python is
# installed elsewhere on PATH).
$python = $null
foreach ($candidate in (Get-Command python -All -ErrorAction SilentlyContinue)) {
    if ($candidate.Source -notlike "*\WindowsApps\python.exe") {
        $python = $candidate.Source
        break
    }
}
if (-not $python) {
    $pyLauncher = Get-Command py -ErrorAction SilentlyContinue
    if ($pyLauncher) {
        $python = "py"
    }
}

# Verify prerequisites
Write-Host "Checking prerequisites..." -ForegroundColor Yellow

if ($python) {
    Write-Host "  ✓ Python 3.12+ ($python)" -ForegroundColor Green
} else {
    Write-Host "  ✗ Python 3.12+ not found (only the Microsoft Store alias was seen on PATH)" -ForegroundColor Red
    exit 1
}

try {
    & dotnet --version 2>&1 | Out-Null
    Write-Host "  ✓ .NET 6+" -ForegroundColor Green
} catch {
    Write-Host "  ✗ .NET 6+ not found in PATH" -ForegroundColor Red
    exit 1
}

if ($bazelisk) {
    Write-Host "  ✓ Bazelisk ($bazelisk)" -ForegroundColor Green
} else {
    Write-Host "  ✗ Bazelisk not found in PATH or at $RepoRoot\bazelisk.exe" -ForegroundColor Red
    exit 1
}

# The installer genrule passes BAZEL_VC to build_installer.py through Bash
# with `set -u`, so it must be defined even when Bazelisk is invoked directly
# instead of through src/bazel/bazel_wrapper/bazel.bat. Reuse the repository's
# Visual Studio resolver and preserve an explicitly supplied value.
if (-not $env:BAZEL_VC) {
    try {
        $vcDir = (& $python "$RepoRoot\src\build_tools\vs_util.py").Trim()
        if ($LASTEXITCODE -ne 0 -or -not $vcDir -or -not (Test-Path "$vcDir\Auxiliary\Build\vcvarsall.bat")) {
            throw "Visual Studio C++ toolchain was not found"
        }
        $env:BAZEL_VC = $vcDir
    } catch {
        Write-Host "  ✗ Visual Studio C++ toolchain not found: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}
Write-Host "  ✓ Visual Studio VC ($env:BAZEL_VC)" -ForegroundColor Green
Write-Host ""

# Change to src directory
Push-Location "$RepoRoot\src"
$srcDir = Get-Location

try {
    # Step 1: Update dependencies
    if (-not $SkipDeps) {
        Write-Host "Step 1: Downloading build dependencies..." -ForegroundColor Yellow
        Write-Host "  (LLVM, MSYS2, Qt, Ninja, .NET tools)" -ForegroundColor Gray
        & $python build_tools/update_deps.py
        if ($LASTEXITCODE -ne 0) {
            throw "update_deps.py failed with exit code $LASTEXITCODE"
        }
        Write-Host "  ✓ Dependencies downloaded" -ForegroundColor Green
        Write-Host ""
    }

    # Step 2: Build Qt
    if (-not $SkipQt) {
        Write-Host "Step 2: Building Qt 6 (may take 15-30 min)..." -ForegroundColor Yellow
        & $python build_tools/build_qt.py --release --confirm_license
        if ($LASTEXITCODE -ne 0) {
            throw "build_qt.py failed with exit code $LASTEXITCODE"
        }
        Write-Host "  ✓ Qt 6 built" -ForegroundColor Green
        Write-Host ""
    }

    # Step 3: Clean Bazel cache if requested
    if ($CleanBuild) {
        Write-Host "Step 3: Cleaning Bazel cache..." -ForegroundColor Yellow
        & $bazelisk clean --expunge
        Write-Host "  ✓ Bazel cache cleaned" -ForegroundColor Green
        Write-Host ""
    }

    # Step 4: Build marinaMoji package
    Write-Host "Step 4: Building marinaMoji (x64)..." -ForegroundColor Yellow
    Write-Host "  Config: oss_windows release_build" -ForegroundColor Gray
    Write-Host "  Target: marinaMoji64.msi" -ForegroundColor Gray
    # Batch mode keeps the build self-contained and prevents an interrupted
    # PowerShell invocation from leaving a stale Bazel server holding the
    # output-base lock for the next build.
    & $bazelisk --batch build --config oss_windows --config release_build package
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
