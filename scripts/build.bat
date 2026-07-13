@echo off
REM Quick batch wrapper to build marinaMoji on Windows (x64)
REM Usage: build.bat [--skip-deps] [--skip-qt] [--clean]

setlocal enabledelayedexpansion

echo.
echo === marinaMoji Windows Build (batch wrapper) ===
echo.

REM Parse arguments
set SKIP_DEPS=
set SKIP_QT=
set CLEAN_BUILD=

:parse_args
if "%~1"=="" goto done_parsing
if "%~1"=="--skip-deps" set SKIP_DEPS=-SkipDeps
if "%~1"=="--skip-qt" set SKIP_QT=-SkipQt
if "%~1"=="--clean" set CLEAN_BUILD=-CleanBuild
shift
goto parse_args

:done_parsing
REM Get script directory
set SCRIPT_DIR=%~dp0

echo Running PowerShell build script...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build_marinamoji_windows.ps1" %SKIP_DEPS% %SKIP_QT% %CLEAN_BUILD%

if errorlevel 1 (
    echo.
    echo Build failed. Check output above for errors.
    pause
    exit /b 1
) else (
    echo.
    echo Build succeeded. Check output for marinaMoji64.msi location.
    pause
)
