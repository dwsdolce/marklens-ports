@echo off
REM Run the Rust port from the current source, building first if needed.
REM
REM   rust\packaging\run_win.bat [document.md] [args...]
REM
REM This is the tool for iterating on the code: `cargo run` rebuilds whatever
REM changed and runs it. A debug build, because that is what iterating wants;
REM packaging\build_win.bat produces the optimised artefact.

cd /d "%~dp0\.."

where cargo >nul 2>&1
if errorlevel 1 (
    echo cargo is not on PATH. Run packaging\setup.bat first.
    exit /b 1
)

pushd src-tauri
cargo run -- %*
set "RC=%errorlevel%"
popd
exit /b %RC%
