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
if not defined CMAKE_PREFIX_PATH (
    echo CMAKE_PREFIX_PATH is not set - point it at your Qt 6 installation, e.g.
    echo   set CMAKE_PREFIX_PATH=C:/Qt/6.8.0/msvc2022_64
    exit /b 1
)

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
REM Generate the Windows icon from the shared PNG
REM ===============================================
"%PYTHON%" ..\tools\make_ico.py
if errorlevel 1 (
    echo Generating the icon failed
    exit /b 1
)

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
cmake -B "%BUILD_DIR%" -S . -DCMAKE_BUILD_TYPE=Release -DMARKLENS_FULL_VERSION=%VERSION%
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
if not defined WINDEPLOYQT if exist "%CMAKE_PREFIX_PATH%\bin\windeployqt.exe" set "WINDEPLOYQT=%CMAKE_PREFIX_PATH%\bin\windeployqt.exe"
if not defined WINDEPLOYQT (
    echo windeployqt not found - add Qt's bin directory to PATH
    exit /b 1
)

"%WINDEPLOYQT%" --release --no-translations --no-compiler-runtime "%STAGE_DIR%\marklens-cpp.exe"
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
