"""PyInstaller entry point.

PyInstaller runs the entry script as a top-level module, so it can't be the
package's own ``__main__.py`` (whose ``from . import ...`` needs a parent
package). This launcher lives outside the package and imports it absolutely.
"""

from marklens.__main__ import main

if __name__ == "__main__":
    raise SystemExit(main())
