@echo off
REM Build the Windows installer for the Rust port.
REM
REM   rust\packaging\build_win.bat
REM
REM Produces:
REM   rust\installer\Marklens_Rust_V<version>.exe
REM
REM Requires Rust (rustup), the Tauri CLI
REM (cargo install tauri-cli --version "^2.0") and Python 3 for the icon and
REM version stamping. The MSVC build tools have to be installed; the WebView2
REM runtime does not - the NSIS installer carries its bootstrapper.
REM
REM Unlike the other two ports this does not drive Inno Setup. Tauri's own
REM bundler builds the NSIS installer, and it is the only one of the three that
REM knows how to detect and install the WebView2 runtime the app cannot start
REM without. The output is renamed to match the other ports.

REM Run from the port's root regardless of where this script is invoked from.
cd /d "%~dp0\.."

if not defined PYTHON set "PYTHON=python"

where cargo >nul 2>&1
if errorlevel 1 (
    echo cargo is not on PATH - install Rust from https://rustup.rs
    exit /b 1
)
cargo tauri --version >nul 2>&1
if errorlevel 1 (
    echo The Tauri CLI is not installed
    echo Run: cargo install tauri-cli --version "^^2.0"
    exit /b 1
)

echo Running build for %PROCESSOR_ARCHITECTURE% architecture

REM ===============================================
REM Clean-up
REM ===============================================
REM The cargo target directory is deliberately left alone - a full rebuild of
REM the Tauri dependency tree costs minutes and nothing in it goes stale
REM between packaging runs. Only the bundle output is cleared.
if exist "src-tauri\target\release\bundle" rmdir /s /q "src-tauri\target\release\bundle"
if exist installer rmdir /s /q installer
mkdir installer

REM ===============================================
REM Generate the Windows icon from the shared PNG
REM ===============================================
REM Tauri picks the .ico out of bundle.icon for the exe and the installer.
"%PYTHON%" ..\tools\make_ico.py ..\shared\icon.png src-tauri\icons\icon.ico
if errorlevel 1 (
    echo Generating the icon failed
    exit /b 1
)

REM ===============================================
REM Generate the version from the git commit count
REM ===============================================
REM The bundle itself carries the three-part version from tauri.conf.json,
REM which Tauri requires to be valid semver; the fourth component is the build
REM number and appears in the installer file name, matching the other ports.
for /f %%v in ('"%PYTHON%" ..\tools\gen_version_build.py rust --print') do set VERSION=%%v
if not defined VERSION (
    echo Generating the version failed
    exit /b 1
)
echo Creating installer for version %VERSION%

REM ===============================================
REM Build and bundle
REM ===============================================
REM NSIS only: the MSI target needs WiX downloaded separately and offers
REM nothing here that NSIS does not.
pushd src-tauri
cargo tauri build --bundles nsis
if errorlevel 1 (
    popd
    echo cargo tauri build failed
    exit /b 1
)
popd

REM ===============================================
REM Collect the installer under the shared naming convention
REM ===============================================
set "BUILT="
for %%f in ("src-tauri\target\release\bundle\nsis\*-setup.exe") do if not defined BUILT set "BUILT=%%f"
if not defined BUILT (
    echo No NSIS installer was produced under src-tauri\target\release\bundle\nsis
    exit /b 1
)

copy /y "%BUILT%" "installer\Marklens_Rust_V%VERSION%.exe" >nul
if errorlevel 1 (
    echo Copying the installer failed
    exit /b 1
)

echo Installer created successfully: rust\installer\Marklens_Rust_V%VERSION%.exe
