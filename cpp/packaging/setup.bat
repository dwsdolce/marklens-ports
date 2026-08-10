@echo off
setlocal enabledelayedexpansion
REM Set the C++ port up so the build scripts run with no further configuration.
REM
REM   cpp\packaging\setup.bat            check, and install what it can
REM   cpp\packaging\setup.bat --check    report only, change nothing
REM
REM The cmd counterpart of packaging/setup; see that script's header for why
REM the requirements are split into prerequisites (verified, never installed
REM here) and dependencies (built into third_party\).

cd /d "%~dp0\.."

set "CHECK_ONLY=0"
if "%~1"=="--check" set "CHECK_ONLY=1"
if not "%~1"=="" if not "%~1"=="--check" (
    echo usage: setup.bat [--check]
    exit /b 1
)

set "THIRD_PARTY=%cd%\third_party"
set "MD4C_SRC=%THIRD_PARTY%\md4c"
set /a MISSING=0

echo.
echo Marklens C++ - build environment
echo.
echo Prerequisites (install these yourself)

REM ===============================================
REM CMake
REM ===============================================
where cmake >nul 2>&1
if errorlevel 1 (
    call :bad "cmake - https://cmake.org/download/"
) else (
    for /f "tokens=3" %%v in ('cmake --version 2^>nul ^| findstr /b "cmake version"') do call :ok "cmake %%v"
)

where git >nul 2>&1
if errorlevel 1 call :bad "git - https://git-scm.com/download/win (needed to fetch md4c)"

REM ===============================================
REM MSVC. CMake locates it itself, so only presence is checked.
REM ===============================================
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSNAME="
if exist "%VSWHERE%" (
    for /f "delims=" %%n in ('"%VSWHERE%" -products * -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName 2^>nul') do set "VSNAME=%%n"
)
if defined VSNAME (
    call :ok "!VSNAME!"
) else (
    call :bad "C++ toolchain (MSVC) - https://visualstudio.microsoft.com/downloads/ with the 'Desktop development with C++' workload"
)

REM ===============================================
REM Qt 6 - the same search CMakeLists.txt does
REM ===============================================
set "QT_KIT="
if defined QTDIR if exist "%QTDIR%\lib\cmake\Qt6Core" set "QT_KIT=%QTDIR%"
if not defined QT_KIT (
    for /f "delims=" %%k in ('dir /b /ad /o-n "C:\Qt" 2^>nul') do (
        if not defined QT_KIT (
            for /f "delims=" %%m in ('dir /b /ad "C:\Qt\%%k" 2^>nul') do (
                if not defined QT_KIT if exist "C:\Qt\%%k\%%m\lib\cmake\Qt6Core" set "QT_KIT=C:\Qt\%%k\%%m"
            )
        )
    )
)

if not defined QT_KIT (
    call :bad "Qt 6 - https://www.qt.io/download-qt-installer (needs an account; tick Qt WebEngine, Qt WebChannel and Qt Positioning under Additional Libraries)"
) else (
    call :ok "Qt 6 at !QT_KIT!"
    set "QT_MISSING="
    for %%m in (Widgets Test WebEngineWidgets WebEngineCore WebChannel Positioning) do (
        if not exist "!QT_KIT!\lib\cmake\Qt6%%m" set "QT_MISSING=!QT_MISSING! %%m"
    )
    if not defined QT_MISSING (
        call :ok "Qt modules: WebEngine, WebChannel, Positioning"
    ) else (
        if "!CHECK_ONLY!"=="0" if exist "C:\Qt\MaintenanceTool.exe" (
            echo   ....     adding Qt modules:!QT_MISSING!
            for %%a in ("!QT_KIT!") do set "QT_KIT_NAME=%%~nxa"
            for %%a in ("!QT_KIT!\..") do set "QT_VERSION=%%~nxa"
            set "QT_TAG=!QT_VERSION:.=!"
            "C:\Qt\MaintenanceTool.exe" --accept-licenses --accept-obligations --default-answer --confirm-command install "extensions.qtwebengine.!QT_TAG!.win64_!QT_KIT_NAME!" "qt.qt6.!QT_TAG!.addons.qtwebchannel" "qt.qt6.!QT_TAG!.addons.qtpositioning" >nul 2>&1
            set "QT_STILL="
            for %%m in (Widgets Test WebEngineWidgets WebEngineCore WebChannel Positioning) do (
                if not exist "!QT_KIT!\lib\cmake\Qt6%%m" set "QT_STILL=!QT_STILL! %%m"
            )
            if not defined QT_STILL (
                call :ok "Qt modules: WebEngine, WebChannel, Positioning (installed)"
            ) else (
                call :bad "Qt modules:!QT_STILL! - run 'C:\Qt\MaintenanceTool.exe search qtwebengine' and install by hand"
            )
        ) else (
            call :bad "Qt modules:!QT_MISSING! - add them with the Qt maintenance tool"
        )
    )
)

REM ===============================================
REM Dependencies
REM ===============================================
echo.
echo Dependencies (this script installs these)

if exist "%MD4C_SRC%\src\md4c-html.h" (
    call :ok "md4c sources in third_party\md4c"
) else if "%CHECK_ONLY%"=="1" (
    call :bad "md4c - re-run without --check to fetch it"
) else (
    echo   ....     fetching md4c sources into third_party\ ...
    if not exist "%THIRD_PARTY%" mkdir "%THIRD_PARTY%"
    if exist "%MD4C_SRC%" rmdir /s /q "%MD4C_SRC%"
    git clone --depth 1 https://github.com/mity/md4c.git "%MD4C_SRC%" >nul 2>&1
    if exist "%MD4C_SRC%\src\md4c-html.h" (
        call :ok "md4c sources in third_party\md4c"
    ) else (
        call :bad "md4c - the clone failed; fetch it by hand or set MD4C_ROOT"
    )
)

REM ===============================================
REM Verdict
REM ===============================================
echo.
if %MISSING% GTR 0 (
    echo %MISSING% thing^(s^) still needed - see the MISSING lines above.
    echo Install them and re-run this script.
    exit /b 1
)

echo Ready. Run it with:
echo     packaging\run_win.bat ..\shared\spec\sample\index.md
echo.
echo That builds first if it needs to, and is the tool for iterating.
echo Test with:
echo     cmake -B build ^&^& cmake --build build --config Release
echo     ctest --test-dir build --output-on-failure -C Release
echo Package with:
echo     packaging\build_win.bat
exit /b 0

:ok
echo   ok       %~1
exit /b 0

:bad
echo   MISSING  %~1
set /a MISSING+=1
exit /b 0
