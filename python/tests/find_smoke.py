"""Regression test for the find bar losing its place across a re-render.

Auto-reload re-renders the document on every save of the file being read. The
page the search ran against is then gone: the highlights go with it and the
count still reads a number for a page that no longer exists, so the arrows and
Return look like they have stopped responding. This asserts the count comes
back by itself once the new page has loaded.

Mirrors cpp/tests/find_smoke.cpp. Run with QT_QPA_PLATFORM=offscreen.
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from marklens.app import MainWindow

DOC = "# Haystack\n\nneedle one\n\nneedle two\n\nneedle three\n"


def main() -> int:
    app = QApplication(sys.argv)

    with tempfile.TemporaryDirectory() as tmp:
        doc = Path(tmp) / "index.md"
        doc.write_text(DOC, encoding="utf-8")

        win = MainWindow()
        win.resize(900, 720)
        win.show()
        win.open_path(doc)

        state = {"loads": 0, "before": "", "after": ""}

        def on_load(ok: bool) -> None:
            if not ok:
                return
            state["loads"] += 1
            if state["loads"] == 1:
                win._focus_find()
                win._find_input.setText("needle")
                win._find_text(False)
                QTimer.singleShot(800, first_search_done)
            else:
                QTimer.singleShot(800, after_rerender)

        def first_search_done() -> None:
            state["before"] = win._find_count.text()
            if state["before"] != "1 of 3":
                app.quit()
                return
            # Blank it so only the window itself can put a count back, then
            # re-render - the same path auto-reload takes on save.
            win._find_count.setText("")
            win._render()

        def after_rerender() -> None:
            state["after"] = win._find_count.text()
            app.quit()

        win._view.loadFinished.connect(on_load)
        QTimer.singleShot(25000, app.quit)  # hard timeout
        app.exec()

        ok = state["before"] == "1 of 3" and state["after"] == "1 of 3"
        print(f"FIND: {'PASS' if ok else 'FAIL'} "
              f"(before the re-render {state['before']!r}, after {state['after']!r})")
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
