@echo off
REM Run the Python port from the current source.
REM
REM   python\packaging\run_win.bat [document.md] [args...]
REM
REM This is the tool for iterating on the code: it runs the working tree,
REM exactly as `python -m marklens` does, and never the frozen bundle under
REM dist\. Use packaging\build_win.bat when you want that instead.
REM
REM There is no build step here - that is the whole point of the port.
REM
REM Which python that is was settled before this script started: the venv you
REM activated, or the one VS Code activated in its terminal. Nothing here goes
REM looking for a venv. A script that picks its own interpreter can pick a
REM different one from the one you have been installing into, and the project
REM keeps a single environment at the repository root precisely so that cannot
REM happen. python\packaging\setup is what checks it.

cd /d "%~dp0\.."

if not defined VIRTUAL_ENV (
    echo No virtual environment is active.
    echo Activate it, then run this again:
    echo     .venv\Scripts\activate       from the repository root
    echo Opening marklens-ports in VS Code activates it in new terminals.
    exit /b 1
)

python -m marklens %*
exit /b %errorlevel%
