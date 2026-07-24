"""Entry point: ``marklens [file.md]``."""

from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QApplication

from . import assets
from .app import MainWindow


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("Marklens")
    app.setOrganizationName("Marklens")  # gives QSettings (recent files) a home
    app.setWindowIcon(QIcon(str(assets.icon_path())))
    window = MainWindow()
    window.show()

    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if args:
        window.open_path(Path(args[0]))

    return app.exec()


if __name__ == "__main__":
    sys.exit(main())