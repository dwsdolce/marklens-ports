"""Entry point: ``marklens [file.md]``."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import cast

from PySide6.QtCore import QEvent, QTimer
from PySide6.QtGui import QFileOpenEvent, QIcon
from PySide6.QtWidgets import QApplication

from . import assets
from .app import MainWindow


class _Application(QApplication):
    """QApplication that opens documents macOS delivers as events.

    A double-clicked or "Open With" document does not arrive in ``sys.argv`` on
    macOS. The OS sends an Apple Event, which Qt turns into a ``FileOpen`` event
    aimed at the application object and discards if nothing handles it - so the
    app comes up on its empty state, looking exactly like a file association
    that was never registered.

    This overrides ``event()`` rather than installing an application-wide event
    filter. A filter on QApplication is called for every event delivered to
    every QObject, and PySide has to build a Python wrapper for each watched
    object in order to make the call; for an object that is mid-construction or
    has no Python type that segfaults inside ``PySide::typeName``. Overriding
    ``event()`` sees only what is addressed to the application itself, which is
    where ``FileOpen`` is sent.

    The event can also arrive before ``exec()``, when opening the document is
    what launched the app, so anything that turns up before the window exists is
    held until it does.
    """

    def __init__(self, argv: list[str]) -> None:
        super().__init__(argv)
        self._window: MainWindow | None = None
        self._pending: Path | None = None

    def set_window(self, window: MainWindow) -> None:
        self._window = window
        if self._pending is not None:
            self._open_document(self._pending)
            self._pending = None

    def _open_document(self, path: Path) -> None:
        """Hand the document to the window's own rule.

        macOS routes every document to the application already running and
        delivers it as an Apple Event. What to do with it is the same question
        the file dialog and Open Recent ask, and is answered in one place.
        """
        assert self._window is not None
        self._window.open_document_request(path)

    def event(self, e: QEvent) -> bool:
        if e.type() == QEvent.Type.FileOpen:
            # Only QFileOpenEvent carries a file, and the type check above is
            # what guarantees this is one.
            path = cast(QFileOpenEvent, e).file()
            if path:
                if self._window is not None:
                    self._open_document(Path(path))
                else:
                    self._pending = Path(path)
            return True
        return super().event(e)


def main() -> int:
    app = _Application(sys.argv)
    app.setApplicationName("Marklens")
    app.setOrganizationName("Marklens")  # gives QSettings (recent files) a home
    app.setWindowIcon(QIcon(str(assets.icon_path())))

    window = MainWindow()
    window.show()

    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if args:
        window.open_path(Path(args[0]))

    app.set_window(window)

    # Nothing named on the command line: pick up where you left off.
    if not args:
        if sys.platform == "darwin":
            # Except that on macOS "nothing named" is also what opening a
            # document from Finder looks like - the path arrives as an Apple
            # Event once the event loop is running, not in argv. Reopening
            # immediately would show the previous document first and push it
            # back to the top of the recent list, so the fallback waits a
            # moment and stands down if a document turns up meanwhile.
            def reopen_last() -> None:
                if not window.has_document():
                    window.open_most_recent()

            QTimer.singleShot(250, reopen_last)
        else:
            window.open_most_recent()

    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
