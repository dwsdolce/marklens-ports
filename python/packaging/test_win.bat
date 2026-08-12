@echo off
REM Run the Python port's tests against the current source.
REM
REM   python\packaging\test_win.bat [pytest args...]
REM
REM This is the tool for testing while working: it tests the working tree,
REM never the frozen bundle under dist\. Anything you pass is handed to pytest,
REM so `packaging\test_win.bat -k renderer` selects a subset.
REM
REM There is no build step - that is the whole point of the port. PYTHONPATH is
REM set because the GUI tests import their helpers from tests\ itself.
REM
REM Which python this uses was settled before the script started: the venv you
REM activated, or the one VS Code activated in its terminal. Nothing here goes
REM looking for a venv, for the same reason run_win.bat does not - a script
REM that picks its own interpreter can pick a different one from the one you
REM have been installing into.

cd /d "%~dp0\.."

if not defined VIRTUAL_ENV (
    echo No virtual environment is active.
    echo Activate it, then run this again:
    echo     .venv\Scripts\activate       from the repository root
    echo Opening marklens-ports in VS Code activates it in new terminals.
    exit /b 1
)

set PYTHONPATH=tests
python -m pytest -q %*
exit /b %errorlevel%
