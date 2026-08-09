@echo off
setlocal
REM Run the C++ port from the current source, building first if needed.
REM
REM   cpp\packaging\run_win.bat [document.md] [args...]
REM
REM This is the tool for iterating on the code: it always runs what is in the
REM working tree, never anything under dist\. Use packaging\build_win.bat when
REM you want the packaged artefact instead.

cd /d "%~dp0\.."

if not exist "build\CMakeCache.txt" (
    echo Configuring build\ ...
    cmake -B build || exit /b 1
)

echo Building ...
cmake --build build --config Release || exit /b 1

set "EXE="
for %%c in ("build\Release\marklens-cpp.exe" "build\marklens-cpp.exe") do (
    if not defined EXE if exist "%%~c" set "EXE=%%~c"
)
if not defined EXE (
    echo Built, but no executable found under build\
    exit /b 1
)

REM A development build has no Qt beside it, so it exits silently with no window
REM and no error unless the DLLs are reachable. Ask CMake which Qt it used:
REM Qt6_DIR is <kit>\lib\cmake\Qt6, putting bin\ three levels up.
where windeployqt.exe >nul 2>&1
if errorlevel 1 (
    for /f "tokens=2 delims==" %%d in ('findstr /b "Qt6_DIR:PATH=" "build\CMakeCache.txt" 2^>nul') do (
        for %%a in ("%%~dpd.") do for %%b in ("%%~dpa.") do for %%g in ("%%~dpb.") do (
            if exist "%%~fg\bin\Qt6Core.dll" set "PATH=%%~fg\bin;%PATH%"
        )
    )
)

echo Running %EXE%
"%EXE%" %*
exit /b %errorlevel%
