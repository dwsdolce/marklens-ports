@echo off
setlocal enabledelayedexpansion
REM Set the Rust port up so the build scripts run with no further configuration.
REM
REM   rust\packaging\setup.bat            check, and install what it can
REM   rust\packaging\setup.bat --check    report only, change nothing
REM
REM The cmd counterpart of packaging/setup. There is little to do: Cargo.toml
REM already is this port's manifest, so `cargo build` fetches every library
REM dependency itself. What Cargo has no slot for is tools - the Tauri CLI is a
REM binary, not a crate this links against.

cd /d "%~dp0\.."

set "CHECK_ONLY=0"
if "%~1"=="--check" set "CHECK_ONLY=1"
if not "%~1"=="" if not "%~1"=="--check" (
    echo usage: setup.bat [--check]
    exit /b 1
)

set /a MISSING=0

echo.
echo Marklens Rust - build environment
echo.
echo Prerequisites (install these yourself)

REM ===============================================
REM Rust toolchain
REM ===============================================
where cargo >nul 2>&1
if errorlevel 1 (
    call :bad "Rust toolchain - https://rustup.rs"
) else (
    for /f "tokens=2" %%v in ('cargo --version 2^>nul') do set "CARGO_VERSION=%%v"
    for /f "tokens=1,2 delims=." %%a in ("!CARGO_VERSION!") do (
        set "CARGO_MAJOR=%%a"
        set "CARGO_MINOR=%%b"
    )
    REM Anything before 1.85 cannot parse the 2024-edition manifests in this
    REM dependency tree; it fails with 'feature edition2024 is required' while
    REM resolving, a long way from anything informative.
    if !CARGO_MAJOR! GTR 1 (
        call :ok "cargo !CARGO_VERSION!"
    ) else if !CARGO_MINOR! GEQ 85 (
        call :ok "cargo !CARGO_VERSION!"
    ) else (
        call :bad "cargo !CARGO_VERSION! is too old for the 2024-edition dependencies - run: rustup update"
    )
)

REM ===============================================
REM System webview. The installer carries the WebView2 bootstrapper, so a miss
REM here only affects running straight out of cargo build.
REM ===============================================
if exist "%ProgramFiles(x86)%\Microsoft\EdgeWebView\Application" (
    call :ok "WebView2 runtime"
) else if exist "%ProgramFiles%\Microsoft\EdgeWebView\Application" (
    call :ok "WebView2 runtime"
) else (
    call :ok "WebView2 runtime not detected - the installer bundles it"
)

REM ===============================================
REM Dependencies
REM ===============================================
echo.
echo Dependencies (this script installs these)

cargo tauri --version >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=2" %%v in ('cargo tauri --version 2^>nul') do call :ok "tauri-cli %%v"
) else if "%CHECK_ONLY%"=="1" (
    call :bad "tauri-cli - re-run without --check to install it"
) else (
    where cargo >nul 2>&1
    if errorlevel 1 (
        call :bad "tauri-cli - needs a Rust toolchain first"
    ) else (
        echo   ....     installing tauri-cli ^(a few minutes, it builds from source^) ...
        cargo install tauri-cli --version "^^2.0" --locked >nul 2>&1
        cargo tauri --version >nul 2>&1
        if errorlevel 1 (
            call :bad "tauri-cli - the install failed; run: cargo install tauri-cli --version ^"^^2.0^" --locked"
        ) else (
            for /f "tokens=2" %%v in ('cargo tauri --version 2^>nul') do call :ok "tauri-cli %%v"
        )
    )
)

REM Cargo.toml covers the rest; warming the cache turns the first build from a
REM long silence into a step that has already reported its progress.
if "%CHECK_ONLY%"=="0" (
    where cargo >nul 2>&1
    if not errorlevel 1 (
        echo   ....     fetching crate dependencies ...
        pushd src-tauri
        cargo fetch >nul 2>&1
        if errorlevel 1 (
            popd
            call :bad "cargo fetch failed - check your network"
        ) else (
            popd
            call :ok "crate dependencies fetched (Cargo.toml)"
        )
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
echo     cd src-tauri ^&^& cargo test
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
