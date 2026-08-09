"""PyInstaller entry point. Not a way to run the app.

PyInstaller executes its entry script as a top-level module, so the entry point
cannot be the package's own ``__main__.py``: that module's ``from . import ...``
needs a parent package and would fail. This launcher sits outside the package
and imports it absolutely instead.

It lives in packaging/ beside marklens.spec, the only thing that refers to it.
It used to be python/run.py, which read like the way to run the port and is not
- for that, use ``packaging/run_win`` (or run_mac, run_linux), or the console
script the venv installs. Running this file directly does nothing those do not.
"""

from marklens.__main__ import main

if __name__ == "__main__":
    raise SystemExit(main())
