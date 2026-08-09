@echo off
REM Build the Windows application bundle and installer for the C++ port.
REM
REM   cpp\packaging\build_win.bat
REM
REM Produces:
REM   cpp\dist\                                    the deployed application
REM   cpp\installer\Marklens_Cpp_V<version>.exe
REM
REM Requires CMake, Visual Studio with the C++ toolset (no generator is forced,
REM so CMake finds the compiler itself - a developer prompt is only needed if
REM you override the generator), Qt 6 *with the WebEngine module* (the base Qt
REM install does not include it - add it with the Qt maintenance tool), md4c
REM (no Windows package; use vcpkg or build https://github.com/mity/md4c from
REM source), Inno Setup 6 (https://jrsoftware.org/isdl.php) and Python 3.
REM
REM Configuration comes from the environment. CMAKE_PREFIX_PATH takes a
REM semicolon-separated list:
REM
REM   set CMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64;C:/md4c
REM   set CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

REM Run from the port's root regardless of where this script is invoked from.
cd /d "%~dp0\.."

if not defined PYTHON set "PYTHON=python"
set "BUILD_DIR=build-packaging"
set "STAGE_DIR=dist"

where cmake >nul 2>&1
if errorlevel 1 (
    echo cmake is not on PATH
    exit /b 1
)
REM CMAKE_PREFIX_PATH is deliberately not required. CMakeLists.txt finds Qt in
REM the online installer's layout and md4c in third_party\ where
REM packaging\setup.bat puts it; setting it still overrides both.

echo Running build for %PROCESSOR_ARCHITECTURE% architecture

REM ===============================================
REM Clean-up
REM ===============================================
REM build\ is deliberately left alone: cpp\README.md tells developers to
REM configure there, and wiping their tree as a side effect of packaging would
REM be rude. Packaging gets its own directories.
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"

REM ===============================================
REM Generate the version from the git commit count
REM ===============================================
REM Writes cpp\build\installer_version, which the Inno Setup script reads.
REM --print also writes it, so this is one call, not two.
for /f %%v in ('"%PYTHON%" ..\tools\gen_version_build.py cpp --print') do set VERSION=%%v
if not defined VERSION (
    echo Generating the version failed
    exit /b 1
)
echo Creating installer for version %VERSION%

REM ===============================================
REM Configure and build
REM ===============================================
cmake -B "%BUILD_DIR%" -S . -DMARKLENS_FULL_VERSION=%VERSION%
if errorlevel 1 (
    echo Configuring failed
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo Building failed
    exit /b 1
)

REM ===============================================
REM Stage the application and its assets
REM ===============================================
cmake --install "%BUILD_DIR%" --config Release --prefix "%STAGE_DIR%"
if errorlevel 1 (
    echo Installing to %STAGE_DIR% failed
    exit /b 1
)

REM ===============================================
REM Deploy the Qt runtime next to the executable
REM ===============================================
REM windeployqt copies the Qt DLLs, the platform and imageformat plugins, and -
REM crucially for this app - QtWebEngineProcess.exe together with its Chromium
REM resources and ICU data.
set "WINDEPLOYQT="
for /f "delims=" %%p in ('where windeployqt.exe 2^>nul') do if not defined WINDEPLOYQT set "WINDEPLOYQT=%%p"
REM Ask the build which Qt it actually used rather than guessing: Qt6_DIR in the
REM cache is <kit>/lib/cmake/Qt6, so windeployqt is three levels up in bin\.
if not defined WINDEPLOYQT for /f "tokens=2 delims==" %%d in ('findstr /b "Qt6_DIR:PATH=" "%BUILD_DIR%\CMakeCache.txt" 2^>nul') do (
    for %%a in ("%%~dpd.") do for %%b in ("%%~dpa.") do for %%c in ("%%~dpb.") do (
        if exist "%%~fc\bin\windeployqt.exe" set "WINDEPLOYQT=%%~fc\bin\windeployqt.exe"
    )
)
REM CMAKE_PREFIX_PATH is a CMake *list*, so it may hold several roots separated
REM by semicolons (Qt plus md4c, say). Quote-split on them and try each bin\ directory.
if not defined WINDEPLOYQT for %%r in ("%CMAKE_PREFIX_PATH:;=" "%") do if not defined WINDEPLOYQT if exist "%%~r\bin\windeployqt.exe" set "WINDEPLOYQT=%%~r\bin\windeployqt.exe"
if not defined WINDEPLOYQT (
    echo windeployqt not found next to the Qt that CMake used, nor on PATH.
    echo Run packaging\setup.bat to check the Qt installation.
    exit /b 1
)

REM --skip-plugin-types: `position` is a GPS backend that Qt Positioning brings
REM along only because QtWebEngine links it for the Geolocation web API, which a
REM Markdown viewer never calls. Its NMEA plugin wants Qt SerialPort, which is
REM not installed, so windeployqt warns that it cannot resolve the dependency
REM and then ships a plugin that could not have loaded anyway. `qmltooling` is
REM the QML debugger, loaded only when a debugger attaches. Neither belongs in a
REM release build.
"%WINDEPLOYQT%" --release --no-translations --no-compiler-runtime --skip-plugin-types position,qmltooling "%STAGE_DIR%\marklens-cpp.exe"
if errorlevel 1 (
    echo windeployqt failed
    exit /b 1
)

REM ===============================================
REM Prepare installer path
REM ===============================================
set "installer_file=%cd%\packaging\marklens-cpp.iss"
set "ISCC_PATH=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

if not exist "%ISCC_PATH%" (
    echo Inno Setup 6 not found at %ISCC_PATH%
    echo Install it from https://jrsoftware.org/isdl.php
    exit /b 1
)

REM ===============================================
REM You must install Inno Setup 6 to build the installer
REM ===============================================
REM No /D argument: the script reads build\installer_version itself,
REM which is also what makes it work from Cygwin and Git Bash.
"%ISCC_PATH%" "%installer_file%"
if errorlevel 1 (
    echo Creating the installer failed
    exit /b 1
)

echo Installer created successfully: cpp\installer\Marklens_Cpp_V%VERSION%.exe
