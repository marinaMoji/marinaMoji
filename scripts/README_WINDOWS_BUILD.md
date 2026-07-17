# marinaMoji Windows Build & Test Scripts

Two self-contained PowerShell scripts for building and testing marinaMoji on Windows (x64).

## Overview

- **`build_marinamoji_windows.ps1`** — Run on an AMD64 machine with build environment
  - Downloads dependencies (LLVM, Qt, Ninja, .NET tools)
  - Builds Qt 6
  - Builds marinaMoji with Bazel
  - Outputs `marinaMoji64.msi`

- **`install_and_test_marinamoji.ps1`** — Run on an AMD64 machine with the built MSI
  - Installs marinaMoji alongside stock Mozc
  - Verifies registry, binaries, service registration
  - Tests side-by-side coexistence
  - Provides manual checklist for Notepad testing

## Prerequisites

### On the build machine

Ensure these are installed and in `PATH`:

- **Visual Studio 2022 Community** (or Professional/Enterprise)
  - Windows 11 SDK
  - MSVC v143 C++ build tools (x64/x86)
  - C++ ATL for v143 (x86 & x64)
- **Python 3.12+**
- **.NET 6+** (for `dotnet` command)
- **Bazelisk** (Bazel wrapper)

### On the test machine

- **Windows 10 or 11 (x64 only)** — ARM64 builds deferred to Phase 1g follow-up
- **Stock Mozc already installed** — for side-by-side verification
- **Administrator privileges** — to install MSI and register Windows service
- **Notepad or any text editor** — for manual IME testing

## Quick Start

### Step 1: Build on AMD64 machine

```powershell
# Clone repo (if you haven't)
git clone <marinaMoji repo>
cd marinaMoji

# Run build script
powershell -ExecutionPolicy Bypass -File scripts/build_marinamoji_windows.ps1
```

The script will:
1. Check prerequisites
2. Download dependencies (~2 GB)
3. Build Qt (~20 min)
4. Build marinaMoji (~10 min, depending on cache)
5. Output path to `marinaMoji64.msi`

**First build takes longer.** Subsequent runs can skip steps:

```powershell
# Skip dependency download (already cached)
powershell -ExecutionPolicy Bypass -File scripts/build_marinamoji_windows.ps1 -SkipDeps

# Skip Qt build
powershell -ExecutionPolicy Bypass -File scripts/build_marinamoji_windows.ps1 -SkipDeps -SkipQt

# Full clean rebuild
powershell -ExecutionPolicy Bypass -File scripts/build_marinamoji_windows.ps1 -CleanBuild
```

### Step 2: Copy MSI to test machine

```powershell
# On build machine, copy the MSI
Copy-Item "src\bazel-bin\win32\installer\marinaMoji64.msi" -Destination "D:\marinaMoji64.msi"
```

Transfer to test machine via USB, network share, or your preferred method.

### Step 3: Install and test on AMD64 machine

```powershell
# Run as Administrator
powershell -ExecutionPolicy Bypass -Command "Start-Process powershell -ArgumentList '-File scripts/install_and_test_marinamoji.ps1 -MsiPath D:\marinaMoji64.msi' -Verb RunAs"
```

Or, if already running as admin:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/install_and_test_marinamoji.ps1 -MsiPath D:\marinaMoji64.msi
```

The script will:
1. Verify stock Mozc is installed
2. Install marinaMoji MSI
3. Check registry, binaries, service registration
4. Verify profile directory (`%LOCALAPPDATA%\marinaMoji`)
5. Print manual test checklist

### Step 4: Manual verification in Notepad

Open Notepad and run through the checklist:

1. **Switch IMEs**: Alt+` → See both Mozc and marinaMoji in menu
2. **marinaMoji kana input**: Select marinaMoji, type `a` → See ぁ candidates
3. **marinaMoji kanji**: Type `kanji` → See 漢字 candidates
4. **Preferences**: Right-click marinaMoji → Settings → Qt tool launches
5. **Stock Mozc still works**: Switch back to Mozc, repeat steps 2–3
6. **Verify profile dir**: `%LOCALAPPDATA%\marinaMoji` exists and has files

### Step 5: (Optional) Uninstall and verify stock Mozc survives

```powershell
powershell -ExecutionPolicy Bypass -File scripts/install_and_test_marinamoji.ps1 `
    -MsiPath D:\marinaMoji64.msi -NoInstall -UninstallAfter
```

Then test stock Mozc one more time in Notepad.

## Troubleshooting

### Build fails: "bazelisk not found"

```powershell
# Install Bazelisk
choco install bazelisk  # Or download from https://github.com/bazelbuild/bazelisk/releases

# Verify
bazelisk version
```

### Build fails: "Visual Studio not found"

The script looks for VS 2022 via environment variables. If build fails, check:

```powershell
# Verify BAZEL_VC is set
$env:BAZEL_VC

# If empty, find VS install:
# C:\Program Files\Microsoft Visual Studio\2022\Community\VC
# Set it:
$env:BAZEL_VC = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC"
```

### Build fails: "Out of disk space"

Qt build can need ~8 GB. Clean previous artifacts:

```powershell
cd src
bazelisk clean --expunge
python build_tools/update_deps.py --cache_only  # Skip re-download
```

### MSI install fails

- Ensure you're running PowerShell **as Administrator**
- Check Windows Event Log (Event Viewer → Windows Logs → Application) for MSI error codes
- If registry keys exist but IME doesn't appear in Settings, try:
  - Restart the computer
  - Run `gpupdate /force` (Group Policy refresh)
  - Check Regional Settings (Settings → Time & Language → Language → Japanese → Options)

### marinaMoji doesn't appear in IME menu after install

- Restart the computer
- Check `HKLM:\SOFTWARE\CRCAO\marinaMoji` exists
- Check `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Keyboard Layouts` for GUID `8D513EAE-75C6-4CCA-A307-90F97E573706`
- See [WINDOWS_PORT_PLAN.md](../docs/WINDOWS_PORT_PLAN.md) § 1g for details

## Debugging

Both scripts output detailed logs. Save output to a file:

```powershell
# Build
powershell -ExecutionPolicy Bypass -File scripts/build_marinamoji_windows.ps1 | Tee-Object build.log

# Test
powershell -ExecutionPolicy Bypass -File scripts/install_and_test_marinamoji.ps1 -MsiPath D:\marinaMoji64.msi | Tee-Object test.log
```

Share `build.log` or `test.log` for debugging.

## Phase 1g Checklist

Use these scripts to verify all Phase 1g requirements:

- [x] `bazelisk build --config oss_windows --config release_build package` succeeds
- [x] `marinaMoji64.msi` produced
- [ ] Install MSI on machine with stock Mozc (script does this)
- [ ] Both appear separately in Settings (manual check in Step 4)
- [ ] marinaMoji language bar icon appears (visual check)
- [ ] Basic kana→kanji conversion works (manual Notepad test)
- [ ] Preferences/Dictionary Tool open (manual check)
- [ ] Profile dir created at `%LOCALAPPDATA%\marinaMoji` (script verifies)
- [ ] Stock Mozc still works (manual Notepad test)
- [ ] Windows tests pass (separate: `bazelisk test ... --config oss_windows`)

---

**Next phase**: Phase 2 (Marina session features on Windows) — verifies Manyōshū, odoriji, number-row shortcuts, etc. that live in shared code.

See [WINDOWS_PORT_PLAN.md](../docs/WINDOWS_PORT_PLAN.md) for full plan.
