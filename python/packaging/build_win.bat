@echo off
REM Build the Windows application bundle and installer for the Python port.
REM
REM   python\packaging\build_win.bat
REM
REM Produces:
REM   python\dist\marklens-py\                        the PyInstaller bundle
REM   python\installer\Marklens_Python_V<version>.exe
REM
REM Requires the project venv to be active (packaging\setup.bat checks it) and
REM Inno Setup 6 (https://jrsoftware.org/isdl.php)

REM Run from the port's root regardless of where this script is invoked from.
cd /d "%~dp0\.."

REM ===============================================
REM  Setup the correct python environment
REM ===============================================
REM The venv is yours to activate - from the shell, or by opening
REM marklens-ports in VS Code, which activates it in new terminals. Nothing
REM here goes looking for one: a script that picks its own interpreter can pick
REM a different one from the one you have been installing into.
REM python\packaging\setup is what checks it.
if not defined VIRTUAL_ENV (
    echo No virtual environment is active.
    echo Activate it, then run this again:
    echo     .venv\Scripts\activate       from the repository root
    exit /b 1
)
echo Using %VIRTUAL_ENV%

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
