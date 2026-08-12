@echo off
setlocal
REM Run the Rust port's tests against the current source.
REM
REM   rust\packaging\test_win.bat [cargo test args...]
REM
REM This is the tool for testing while working: `cargo test` rebuilds whatever
REM changed and runs it, against the working tree rather than anything under
REM dist\. Anything you pass is handed to cargo, so
REM `packaging\test_win.bat link_cases` selects a single test.

cd /d "%~dp0\.."

where cargo >nul 2>&1
if errorlevel 1 (
    echo cargo is not on PATH. Run packaging\setup.bat first.
    exit /b 1
)

cd src-tauri
cargo test %*
exit /b %errorlevel%
