@echo off
REM Run the Python port, however it is available.
REM
REM   python\packaging\run_win.bat [document.md] [args...]
REM
REM The cmd counterpart of packaging/run_win. This port has no compile step, so
REM the two candidates are not two builds: the frozen PyInstaller bundle from
REM `build_win.bat app`, or the source run from the venv.

cd /d "%~dp0\.."

if exist "dist\marklens-py\marklens-py.exe" (
    "dist\marklens-py\marklens-py.exe" %*
    exit /b %errorlevel%
)

if exist ".venv\Scripts\python.exe" (
    ".venv\Scripts\python.exe" -m marklens %*
    exit /b %errorlevel%
)
if exist "..\.venv\Scripts\python.exe" (
    "..\.venv\Scripts\python.exe" -m marklens %*
    exit /b %errorlevel%
)

echo Nothing to run. Either set up the venv:
echo     python -m venv .venv ^&^& .venv\Scripts\pip install -e ".[packaging]"
echo or build the frozen bundle:
echo     packaging\build_win.bat app
exit /b 1
