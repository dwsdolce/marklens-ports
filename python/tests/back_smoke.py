"""Back after an in-page anchor must return to where the link was read.

History used to hold documents only, so following "#section" and pressing Back
reopened the previous *document* instead of coming back to the position the link
was on. Drives the real MainWindow offscreen: scroll, click the anchor, press
Back, read the offset back out. Exit 0 = pass.
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from marklens.app import MainWindow

# The link sits well down the document and its target further still, so both a
# successful jump and a successful return are unambiguous.
DOC = (
    "# Top\n\n"
    + "filler\n\n" * 120
    + "[jump](#the-target)\n\n"
    + "filler\n\n" * 200
    + "## The Target\n\ncontent\n"
)


def main() -> int:
    folder = Path(tempfile.mkdtemp())
    doc = folder / "doc.md"
    doc.write_text(DOC, encoding="utf-8")

    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(900, 720)
    window.show()
    window.open_path(doc)

    state: dict[str, object] = {}

    def js(code: str, then) -> None:
        window._view.page().runJavaScript(code, then)

    def scrolled_down(value: object) -> None:
        state["before"] = float(value or 0)
        js("document.querySelector('a[href=\"#the-target\"]').click(); 1",
           lambda _: QTimer.singleShot(600, read_jump))

    def read_jump() -> None:
        js("document.scrollingElement.scrollTop", after_jump)

    def after_jump(value: object) -> None:
        state["jump"] = float(value or 0)
        state["back_enabled"] = window._back_action.isEnabled()
        window._go_back()
        QTimer.singleShot(700, lambda: js("document.scrollingElement.scrollTop", after_back))

    def after_back(value: object) -> None:
        state["back"] = float(value or 0)
        app.quit()

    def start(ok: bool) -> None:
        if not ok:
            print("BACK: FAIL (document did not load)")
            app.quit()
            return
        QTimer.singleShot(
            600,
            lambda: js(
                "document.scrollingElement.scrollTop = 3000;"
                " document.scrollingElement.scrollTop",
                scrolled_down,
            ),
        )

    window._view.loadFinished.connect(start)
    QTimer.singleShot(30000, app.quit)
    app.exec()

    before = float(state.get("before", 0) or 0)
    jump = float(state.get("jump", 0) or 0)
    back = float(state.get("back", -1) or -1)
    ok = jump > before and abs(back - before) < 5 and state.get("back_enabled") is True
    print(
        f"BACK: {'PASS' if ok else 'FAIL'} (before {before:.0f}, jump {jump:.0f}, "
        f"back {back:.0f}, button {state.get('back_enabled')})"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
