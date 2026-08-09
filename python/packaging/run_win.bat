@echo off
REM Run the Python port from the current source.
REM
REM   python\packaging\run_win.bat [document.md] [args...]
REM
REM This is the tool for iterating on the code: it runs the working tree through
REM the venv, exactly as `python -m marklens` does, and never the frozen bundle
REM under dist\. Use packaging\build_win.bat when you want that instead.
REM
REM There is no build step here - that is the whole point of the port.

cd /d "%~dp0\.."

if exist ".venv\Scripts\python.exe" (
    echo Running the source from .venv
    ".venv\Scripts\python.exe" -m marklens %*
    exit /b %errorlevel%
)
if exist "..\.venv\Scripts\python.exe" (
    echo Running the source from ..\.venv
    "..\.venv\Scripts\python.exe" -m marklens %*
    exit /b %errorlevel%
)

echo No virtual environment at python\.venv or ..\.venv
echo Create one, then: pip install -e ".[dev]"
exit /b 1
