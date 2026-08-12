@echo off
setlocal
REM Run the C++ port's tests against the current source, building first if
REM needed.
REM
REM   cpp\packaging\test_win.bat [ctest args...]
REM
REM This is the tool for testing while working: it always tests what is in the
REM working tree, never anything under dist\. Anything you pass is handed to
REM ctest, so `packaging\test_win.bat -R core` selects a single test.
REM
REM It exists for the same reason run_win.bat does. Visual Studio is a
REM multi-config generator: one build\ tree holds every configuration, so both
REM the build and the test have to name one. Forgetting -C does not fail
REM usefully - ctest reports "no tests were found" and exits, which reads like
REM a broken checkout rather than a missing flag.

cd /d "%~dp0\.."

where cmake >nul 2>&1
if errorlevel 1 (
    echo cmake is not on PATH. Run packaging\setup.bat first.
    exit /b 1
)

if not exist "build\CMakeCache.txt" (
    echo Configuring build\ ...
    cmake -B build || exit /b 1
)

echo Building ...
cmake --build build --config Release || exit /b 1

echo Testing ...
ctest --test-dir build --output-on-failure -C Release %*
exit /b %errorlevel%
