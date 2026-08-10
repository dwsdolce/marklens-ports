@echo off
REM Check the Python port's build environment, and install what pyproject.toml
REM describes into it.
REM
REM   python\packaging\setup.bat            check, and install what it can
REM   python\packaging\setup.bat --check    report only, change nothing
REM
REM The same checks as the `setup` shell script, for a cmd or PowerShell
REM prompt. pyproject.toml covers every library dependency; what a manifest
REM cannot cover is the environment around it:
REM
REM   Prerequisites  An activated virtual environment, at the repository root,
REM                  running a Python new enough for requires-python. Creating
REM                  and activating it are yours to do - VS Code does both if
REM                  you open marklens-ports as the workspace folder. This
REM                  script does neither. It is the only script that even
REM                  looks: the build and run scripts use the interpreter they
REM                  are given, so the venv you activate is the venv everything
REM                  uses, and there is no second one to disagree about.
REM
REM   Dependencies   pip install -e ".[dev]" into it.
REM
REM   Packaging      Inno Setup, for build_win.bat. Reported, never required.

setlocal enabledelayedexpansion
cd /d "%~dp0\.."

set "CHECK_ONLY=0"
if "%~1"=="--check" (
    set "CHECK_ONLY=1"
) else if not "%~1"=="" (
    echo usage: setup.bat [--check]
    exit /b 1
)

set /a MISSING=0

echo.
echo Marklens Python - build environment
echo.
echo Prerequisites ^(yours to set up^)

REM ===============================================
REM One environment, at the repository root
REM ===============================================
REM One venv for the whole project rather than one per port. Two is not merely
REM wasteful: the moment they both exist, one of them gets installed into and
REM the other gets run, and the difference shows up as an import error
REM somewhere unrelated. python\.venv is the one that appears by accident -
REM VS Code opened on python\ instead of on marklens-ports - so it is named.
if exist ".venv" (
    call :bad "a second virtual environment exists at python\.venv"
    echo              Remove it. The project keeps one environment, at the
    echo              repository root, so that installing and running cannot end
    echo              up in different places. If it is the only venv you have,
    echo              make the root one first and then delete this.
)

REM Both paths go through %%~fi so they can be compared as strings: it resolves
REM the "..", and appending "\." first makes a trailing separator on
REM VIRTUAL_ENV normalise away. Comparing the last character against a
REM backslash directly would be the obvious way to strip it, but "\" in a
REM batch string comparison unbalances cmd's quote parsing and the line dies
REM with "The syntax of the command is incorrect".
for %%i in ("..\.venv") do set "ROOT_VENV=%%~fi"
set "ACTIVE=%VIRTUAL_ENV%"
if defined ACTIVE for %%i in ("%ACTIVE%\.") do set "ACTIVE=%%~fi"

if not defined ACTIVE (
    call :bad "no virtual environment is active"
    echo              Create it at the repository root, if it is not there yet.
    echo              In VS Code: open marklens-ports as the workspace folder,
    echo              then Command Palette ^> Python: Create Environment ^> Venv.
    echo              By hand, from the repository root:  python -m venv .venv
    echo              Then activate it:  .venv\Scripts\activate
    echo              VS Code activates it for you in new terminals.
) else if /i "%ACTIVE%"=="%ROOT_VENV%" (
    call :ok "virtual environment at the repository root, activated"
) else (
    call :bad "the active virtual environment is not the project's"
    echo              active:    %ACTIVE%
    echo              expected:  %ROOT_VENV%
    echo              Deactivate that one and activate the project's, so that
    echo              what you install and what you run are the same thing.
)

REM ===============================================
REM An interpreter new enough for pyproject.toml
REM ===============================================
REM The active venv's python, not a search of PATH: which interpreter the venv
REM was built with is what decides this, and it cannot be changed afterwards -
REM a too-old venv has to be deleted and remade with a newer python.
if not defined ACTIVE (
    call :skip "python 3.12+ - needs an active virtual environment first"
) else (
    set "PYVER="
    for /f "delims=" %%v in ('python -c "import sys; print(str(sys.version_info[0])+chr(46)+str(sys.version_info[1])+chr(46)+str(sys.version_info[2]))" 2^>nul') do set "PYVER=%%v"
    if not defined PYVER (
        call :bad "python - the active environment has no python on PATH"
    ) else (
        REM 3.12 is what pyproject.toml's requires-python asks for.
        python -c "import sys; sys.exit(0 if sys.version_info >= (3, 12) else 1)" 2>nul
        if errorlevel 1 (
            call :bad "python !PYVER! is too old"
            echo              pyproject.toml asks for 3.12 or newer, and a venv keeps
            echo              the interpreter it was made with. Install a newer
            echo              python, delete the venv at the repository root, and
            echo              make it again with that one:
            echo                  C:\Path\To\python.exe -m venv .venv
            echo              https://www.python.org/downloads/
        ) else (
            call :ok "python !PYVER!"
        )
    )
)

REM ===============================================
REM Packaging tools - reported, never required
REM ===============================================
REM build_win.bat needs this; building and running do not. A missing one is
REM only a problem at the moment you package, and the build script says so
REM again then.
echo.
echo Packaging ^(only needed for build_win.bat^)

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if exist "%ISCC%" (
    call :ok "Inno Setup 6"
) else (
    call :note "Inno Setup 6 - needed for the installer:  https://jrsoftware.org/isdl.php"
)

REM ===============================================
REM What pyproject.toml puts in the venv
REM ===============================================
echo.
echo Dependencies ^(this script installs these^)

if not defined ACTIVE (
    call :skip "marklens - needs an active virtual environment first"
    goto :verdict
)

REM Installed, and with the dev extra? pyinstaller and pytest are what that
REM extra adds, so importing them is the cheapest way to ask.
python -c "import marklens, PyInstaller, pytest" >nul 2>&1
if not errorlevel 1 (
    call :ok "marklens installed with the dev extra"
    goto :verdict
)
if "%CHECK_ONLY%"=="1" (
    call :bad "marklens - re-run without --check to install it"
    goto :verdict
)

echo   ....     pip install -e ".[dev]" ...
python -m pip install -q -e ".[dev]" >nul 2>&1
python -c "import marklens, PyInstaller, pytest" >nul 2>&1
if not errorlevel 1 (
    call :ok "marklens installed with the dev extra"
) else (
    call :bad "marklens - the install failed"
    echo              python -m pip install -e ".[dev]"
)

REM ===============================================
REM Verdict
REM ===============================================
:verdict
echo.
if %MISSING% GTR 0 (
    echo %MISSING% thing^(s^) still needed - see the MISSING lines above.
    echo Sort them out and re-run this script.
    exit /b 1
)

echo Ready. Run it with:
echo     packaging\run_win.bat ..\shared\spec\sample\index.md
echo.
echo That runs the working tree; there is no build step.
echo Test with:
echo     set PYTHONPATH=tests ^&^& python -m pytest tests -q
echo Package with:
echo     packaging\build_win.bat
exit /b 0

:ok
echo   ok       %~1
exit /b 0

:bad
echo   MISSING  %~1
set /a MISSING+=1
exit /b 0

:note
REM Absent but not fatal: wanted by build_win.bat, not by building or running.
echo   --       %~1
exit /b 0

:skip
REM Not asked, because an earlier failure makes the answer meaningless.
echo   SKIPPED  %~1
exit /b 0
