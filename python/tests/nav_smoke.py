"""Regression test for the link-navigation trace trap. Drives the REAL
MainWindow (its queued open_document connection), clicks the sample's relative
link, and verifies the viewer navigated to the target instead of trapping. If
the connection ever regresses to direct, this crashes (SIGTRAP) and fails.
Run with QT_QPA_PLATFORM=offscreen. Exits 0 on PASS.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from marklens.app import MainWindow

SAMPLE = Path(__file__).resolve().parents[2] / "shared" / "spec" / "sample" / "index.md"


def main() -> int:
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(900, 720)
    window.show()
    window.open_path(SAMPLE)

    loads = 0

    def on_load(_ok: bool) -> None:
        nonlocal loads
        loads += 1
        if loads == 1:
            QTimer.singleShot(
                500,
                lambda: window._view.page().runJavaScript(
                    "var a=document.querySelector('a[href=\\'OTHER.md\\']'); a && a.click();"
                ),
            )
        else:
            ok = "OTHER" in window.windowTitle()
            print(f"NAV: {'PASS' if ok else 'FAIL'} (title: {window.windowTitle()})", flush=True)
            os._exit(0 if ok else 1)

    window._view.loadFinished.connect(on_load)
    QTimer.singleShot(12000, lambda: (print("NAV: FAIL (timeout)", flush=True), os._exit(3)))
    return app.exec()


if __name__ == "__main__":
    main()
