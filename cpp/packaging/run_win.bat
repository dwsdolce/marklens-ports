@echo off
setlocal enabledelayedexpansion
REM Run the C++ port, whichever way it was built.
REM
REM   cpp\packaging\run_win.bat [document.md] [args...]
REM
REM The cmd counterpart of packaging/run_win. Two builds can exist side by side
REM and this prefers the deployed one:
REM
REM   dist\    packaging\build_win.bat - carries the Qt runtime, so it runs
REM            with nothing on PATH
REM   build\   a plain `cmake --build` - just the executable, so Qt's DLLs have
REM            to be found first. Without them it exits silently, with no
REM            window and no error, which is why this sets PATH rather than
REM            leaving you to discover that.

cd /d "%~dp0\.."

if exist "dist\marklens-cpp.exe" (
    "dist\marklens-cpp.exe" %*
    exit /b %errorlevel%
)

set "EXE="
for %%c in ("build\Release\marklens-cpp.exe" "build\Debug\marklens-cpp.exe" "build\marklens-cpp.exe") do (
    if not defined EXE if exist "%%~c" set "EXE=%%~c"
)

if not defined EXE (
    echo No build found. Build one first:
    echo     packaging\build_win.bat app
    echo     cmake -B build ^&^& cmake --build build --config Release
    exit /b 1
)

REM Ask CMake where Qt came from - Qt6_DIR is <kit>\lib\cmake\Qt6, so the DLLs
REM are three levels up in bin\.
where windeployqt.exe >nul 2>&1
if errorlevel 1 (
    for %%f in ("build\CMakeCache.txt" "build-packaging\CMakeCache.txt") do (
        if exist "%%~f" for /f "tokens=2 delims==" %%d in ('findstr /b "Qt6_DIR:PATH=" "%%~f" 2^>nul') do (
            for %%a in ("%%~dpd.") do for %%b in ("%%~dpa.") do for %%g in ("%%~dpb.") do (
                if exist "%%~fg\bin\Qt6Core.dll" set "PATH=%%~fg\bin;!PATH!"
            )
        )
    )
)

"%EXE%" %*
exit /b %errorlevel%
