@echo off
REM Run the Rust port, whichever way it was built.
REM
REM   rust\packaging\run_win.bat [document.md] [args...]
REM
REM The cmd counterpart of packaging/run_win. The frontend is embedded in the
REM binary and the webview comes from the OS, so a release build runs as-is.

cd /d "%~dp0\.."

if exist "src-tauri\target\release\marklens-rust.exe" (
    "src-tauri\target\release\marklens-rust.exe" %*
    exit /b %errorlevel%
)
if exist "src-tauri\target\debug\marklens-rust.exe" (
    "src-tauri\target\debug\marklens-rust.exe" %*
    exit /b %errorlevel%
)

where cargo >nul 2>&1
if not errorlevel 1 (
    echo No build found - building and running with cargo.
    pushd src-tauri
    cargo run --release -- %*
    set "RC=%errorlevel%"
    popd
    exit /b %RC%
)

echo No build found and cargo is not on PATH. Run packaging\setup.bat first.
exit /b 1
