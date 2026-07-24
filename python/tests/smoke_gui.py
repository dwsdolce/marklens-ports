"""Headless smoke test: load the sample doc offscreen and assert the page
actually rendered — image resolved, mermaid diagram processed, code highlighted.
Run with QT_QPA_PLATFORM=offscreen.
"""

from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from marklens.app import MainWindow

SAMPLE = Path(__file__).resolve().parents[2] / "shared" / "spec" / "sample" / "index.md"

CHECK_JS = """
(function () {
    var img = document.querySelector('img');
    var mermaid = document.querySelector('.mermaid');
    var hljs = document.querySelector('pre code.hljs, pre code[class*="language-"]');
    return JSON.stringify({
        title: document.title,
        h1: (document.querySelector('h1') || {}).textContent || null,
        imgSrc: img ? img.getAttribute('src') : null,
        imgComplete: img ? (img.complete && img.naturalWidth > 0) : null,
        hasMermaidDiv: !!mermaid,
        mermaidRendered: mermaid ? mermaid.querySelector('svg') !== null : false,
        hasTable: !!document.querySelector('table'),
        codeHighlighted: !!hljs
    });
})();
"""


def main() -> int:
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    win.open_path(SAMPLE)

    result: dict = {}

    def check() -> None:
        win._view.page().runJavaScript(CHECK_JS, 0, on_result)

    def on_result(value: str) -> None:
        import json

        result.update(json.loads(value))
        app.quit()

    # mermaid.js is heavy; give it time to draw the SVG before we inspect.
    win._view.loadFinished.connect(lambda ok: QTimer.singleShot(2500, check))
    QTimer.singleShot(12000, app.quit)  # hard timeout
    app.exec()

    print("RESULT:", result)
    ok = (
        result.get("h1") == "Marklens sample"
        and result.get("imgSrc") == "design/icon.svg"
        and result.get("imgComplete") is True
        and result.get("hasMermaidDiv") is True
        and result.get("hasTable") is True
        and result.get("codeHighlighted") is True
    )
    print("SMOKE:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())