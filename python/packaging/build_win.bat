@echo off
REM Build the Windows application bundle and installer for the Python port.
REM
REM   python\packaging\build_win.bat
REM
REM Produces:
REM   python\dist\marklens-py\                        the PyInstaller bundle
REM   python\installer\Marklens_Python_V<version>.exe
REM
REM Requires Inno Setup 6 (https://jrsoftware.org/isdl.php) and a venv with the
REM packaging extra installed: pip install -e ".[packaging]"

REM Run from the port's root regardless of where this script is invoked from.
cd /d "%~dp0\.."

REM ===============================================
REM  Setup the correct python environment
REM ===============================================
REM Two locations are accepted: python\.venv, which python\README.md describes,
REM and a shared .venv at the repository root, which is convenient when working
REM on more than one port at a time.
set "VENV="
if exist ".venv\Scripts\activate.bat"    set "VENV=.venv"
if not defined VENV if exist "..\.venv\Scripts\activate.bat" set "VENV=..\.venv"

if not defined VENV (
    echo No virtual environment at python\.venv or ..\.venv
    echo Create one, then: pip install -e ".[packaging]"
    exit /b 1
)

echo Activating virtual environment %VENV%
call "%VENV%\Scripts\activate.bat"
if errorlevel 1 (
    echo Activating virtual environment failed
    exit /b 1
)

echo Running build for %PROCESSOR_ARCHITECTURE% architecture

REM ===============================================
REM Clean-up
REM ===============================================
REM build\ holds both the PyInstaller work directory and installer_version,
REM which is regenerated below; removing the lot keeps a stale version out of
REM the installer name.
if exist dist rmdir /s /q dist
if exist build rmdir /s /q build

REM ===============================================
REM Generate the version from the git commit count
REM ===============================================
REM Writes python\build\installer_version, which both the spec file and the
REM Inno Setup script read. --print also writes it, so this is one call.
for /f %%v in ('python ..\tools\gen_version_build.py python --print') do set VERSION=%%v
if not defined VERSION (
    echo Generating the version failed
    exit /b 1
)
echo Creating installer for version %VERSION%

REM ===============================================
REM Run pyinstaller
REM ===============================================
pyinstaller -y packaging\marklens.spec
if errorlevel 1 (
    echo Running pyinstaller failed
    exit /b 1
)

REM ===============================================
REM Prepare installer path
REM ===============================================
set "installer_file=%cd%\packaging\marklens-py.iss"
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

echo Installer created successfully: python\installer\Marklens_Python_V%VERSION%.exe
