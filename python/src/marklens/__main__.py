"""Entry point: ``marklens [file.md]``."""

from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtCore import QEvent, QObject
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QApplication

from . import assets
from .app import MainWindow


class _DocumentOpener(QObject):
    """Open documents macOS hands over as events rather than arguments.

    A double-clicked or "Open With" document does not arrive in ``sys.argv`` on
    macOS. The OS sends an Apple Event, which Qt delivers as a ``FileOpen``
    event to the application object and discards if nothing is listening. The
    app then comes up showing its empty state, which is indistinguishable from
    a file association that was never registered - though the association is
    fine and only the event went unhandled.

    The event can also arrive before ``exec()``, when opening the document is
    what launched the app, so anything that turns up before the window exists is
    held until it does.
    """

    def __init__(self) -> None:
        super().__init__()
        self._window: MainWindow | None = None
        self._pending: Path | None = None

    def set_window(self, window: MainWindow) -> None:
        self._window = window
        if self._pending is not None:
            window.open_path(self._pending)
            self._pending = None

    def eventFilter(self, watched: QObject, event: QEvent) -> bool:
        if event.type() == QEvent.Type.FileOpen:
            path = event.file()
            if path:
                if self._window is not None:
                    self._window.open_path(Path(path))
                else:
                    self._pending = Path(path)
            return True
        return super().eventFilter(watched, event)


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("Marklens")
    app.setOrganizationName("Marklens")  # gives QSettings (recent files) a home
    app.setWindowIcon(QIcon(str(assets.icon_path())))

    # Installed before the window is built, so a launch-by-document event that
    # arrives during construction is caught rather than missed.
    opener = _DocumentOpener()
    app.installEventFilter(opener)

    window = MainWindow()
    window.show()

    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if args:
        window.open_path(Path(args[0]))

    opener.set_window(window)

    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
