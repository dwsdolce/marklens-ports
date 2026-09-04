@echo off
REM Build the Windows installer for the Rust port.
REM
REM   rust\packaging\build_win.bat
REM
REM Produces:
REM   rust\installer\Marklens_Rust_V<version>.exe
REM
REM Requires Rust (rustup), Inno Setup 6 and Python 3 for the icon and version
REM stamping. The MSVC build tools have to be installed; the WebView2 runtime
REM does not - the installer downloads Microsoft's bootstrapper if it is absent.
REM
REM Inno Setup, like the other two ports, rather than Tauri's own NSIS bundler:
REM the bundler's version must be three-part semver, so the build number never
REM reached the installer and every build looked like the same version to it,
REM which meant it could never offer to replace an existing install.
REM packaging\marklens-rust.iss explains the trade in full.

REM Run from the port's root regardless of where this script is invoked from.
cd /d "%~dp0\.."

if not defined PYTHON set "PYTHON=python"

where cargo >nul 2>&1
if errorlevel 1 (
    echo cargo is not on PATH - install Rust from https://rustup.rs
    exit /b 1
)

echo Running build for %PROCESSOR_ARCHITECTURE% architecture

REM ===============================================
REM Clean-up
REM ===============================================
REM The cargo target directory is deliberately left alone - a full rebuild of
REM the Tauri dependency tree costs minutes and nothing in it goes stale
REM between packaging runs. Only the installer output is cleared.
if exist installer rmdir /s /q installer
mkdir installer

REM ===============================================
REM Generate the version from the git commit count
REM ===============================================
REM Four-part, as the other two ports use: the Inno script reads
REM rust\build\installer_version and puts the whole thing in the installer name,
REM the version resource and the Add/Remove Programs entry. tauri.conf.json
REM keeps its own three-part semver, which is all Tauri will accept and all the
REM macOS and Linux bundles need.
for /f %%v in ('"%PYTHON%" ..\tools\gen_version_build.py rust --print') do set VERSION=%%v
if not defined VERSION (
    echo Generating the version failed
    exit /b 1
)
echo Creating installer for version %VERSION%

REM ===============================================
REM Build the executable
REM ===============================================
REM One self-contained binary: the frontend is embedded at compile time and the
REM webview is the operating system's, so there is nothing to stage beside it.
pushd src-tauri
cargo build --release
if errorlevel 1 (
    popd
    echo cargo build failed
    exit /b 1
)
popd

REM ===============================================
REM Build the installer
REM ===============================================
set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo Inno Setup 6 not found at "%ISCC%"
    echo Install it from https://jrsoftware.org/isdl.php
    exit /b 1
)

REM No /D argument: the script reads build\installer_version itself.
"%ISCC%" "packaging\marklens-rust.iss"
if errorlevel 1 (
    echo Creating the installer failed
    exit /b 1
)

echo Installer created successfully: rust\installer\Marklens_Rust_V%VERSION%.exe
