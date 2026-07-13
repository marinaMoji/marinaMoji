@echo off
REM Quick batch wrapper to install and test marinaMoji
REM Usage: test.bat <path-to-MSI> [--no-install] [--uninstall-after]

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Usage: test.bat ^<path-to-MSI^> [--no-install] [--uninstall-after]
    echo Example: test.bat D:\marinaMoji64.msi
    echo.
    exit /b 1
)

set MSI_PATH=%~1
set EXTRA_ARGS=

REM Shift to parse remaining args
shift
:parse_args
if "%~1"=="" goto done_parsing
if "%~1"=="--no-install" set EXTRA_ARGS=!EXTRA_ARGS! -NoInstall
if "%~1"=="--uninstall-after" set EXTRA_ARGS=!EXTRA_ARGS! -UninstallAfter
shift
goto parse_args

:done_parsing
set SCRIPT_DIR=%~dp0

echo.
echo === marinaMoji Installation and Test ===
echo MSI: %MSI_PATH%
echo.

REM Check if running as admin; if not, re-run with elevation
openfiles >nul 2>&1
if errorlevel 1 (
    echo This script requires administrator privileges.
    echo Attempting to elevate...
    powershell -Command "Start-Process powershell -ArgumentList '-File \"%SCRIPT_DIR%install_and_test_marinamoji.ps1\" -MsiPath \"%MSI_PATH%\" %EXTRA_ARGS%' -Verb RunAs"
    exit /b 0
)

REM Already admin, run test
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%install_and_test_marinamoji.ps1" -MsiPath "%MSI_PATH%" %EXTRA_ARGS%

if errorlevel 1 (
    echo.
    echo Test completed with errors. See output above.
    pause
    exit /b 1
) else (
    echo.
    echo Test completed. Review output above for next steps.
    pause
)
