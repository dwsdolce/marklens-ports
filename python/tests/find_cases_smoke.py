"""The shared find contract, driven through the real find bar.

``shared/spec/fixtures/find_cases.json`` says what each search should find in
``find_doc.md``. This runs every case the way a reader would - typing in the
field and ticking the boxes - so it covers the bar's wiring as well as the
engine underneath it. The C++ port runs the same file.

Run with QT_QPA_PLATFORM=offscreen.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from marklens.app import MainWindow

FIXTURES = Path(__file__).resolve().parents[2] / "shared" / "spec" / "fixtures"


def main() -> int:
    spec = json.loads((FIXTURES / "find_cases.json").read_text(encoding="utf-8"))
    cases = spec["cases"]

    app = QApplication(sys.argv)
    win = MainWindow()
    win.resize(900, 720)
    win.show()
    win.open_path(FIXTURES / spec["doc"])
    win._focus_find()

    failures: list[str] = []
    state = {"at": -1, "loaded": False}

    def next_case() -> None:
        state["at"] += 1
        if state["at"] >= len(cases):
            panel_checks()
            return
        case = cases[state["at"]]
        win._find_case.setChecked(bool(case.get("caseSensitive")))
        win._find_regex.setChecked(bool(case.get("regex")))
        # Whole words is disabled while a regex is in force, and a disabled box
        # cannot answer for the query, so the regex cases say so themselves.
        win._find_word.setChecked(bool(case.get("wholeWord")))
        win._find_input.setText(case["phrase"])
        # Everything is synchronous in the page, but the result comes back
        # through runJavaScript, which is not.
        QTimer.singleShot(250, check)

    def check() -> None:
        case = cases[state["at"]]
        shown = win._find_count.text()
        if case["phrase"] == "":
            got, err = 0, False
        elif shown == "No matches":
            got, err = 0, False
        elif shown == "Bad pattern":
            got, err = 0, True
        else:
            got, err = int(shown.split(" of ")[1].rstrip("+")), False

        want, want_err = case["count"], bool(case.get("error"))
        if got != want or err != want_err:
            failures.append(
                f"{case['why']}: wanted {want}{' +error' if want_err else ''}, "
                f"got {got}{' +error' if err else ''} ({shown!r})"
            )
        next_case()

    # -- the panel: the options and the list are the point of all this -------

    def panel_checks() -> None:
        win._find_case.setChecked(False)
        win._find_regex.setChecked(False)
        win._find_word.setChecked(False)
        win._find_wrap.setChecked(True)
        win._find_options_button.setChecked(True)  # opens the panel
        win._find_input.setText("needle")
        QTimer.singleShot(400, panel_filled)

    def panel_filled() -> None:
        rows = win._find_results.count()
        if rows != 4:
            failures.append(f"results list: wanted 4 rows, got {rows}")
        elif "needle" not in win._find_results.item(0).text():
            failures.append(
                f"results list: row 0 does not show the match "
                f"({win._find_results.item(0).text()!r})"
            )
        if win._find_count.text() != "1 of 4":
            failures.append(f"panel: count reads {win._find_count.text()!r}, wanted '1 of 4'")

        # A regex owns its own word boundaries, so the box is refused rather
        # than silently wrapping somebody's pattern.
        win._find_regex.setChecked(True)
        if win._find_word.isEnabled():
            failures.append("whole words stayed enabled while a regex was in force")
        win._find_regex.setChecked(False)

        # Stepping past the last match with wrap off stays put; with it on,
        # it comes round to the first.
        win._find_wrap.setChecked(False)
        QTimer.singleShot(300, no_wrap_check)

    def no_wrap_check() -> None:
        for _ in range(6):  # more steps than there are matches
            win._find_text(False)
        QTimer.singleShot(400, no_wrap_result)

    def no_wrap_result() -> None:
        if win._find_count.text() != "4 of 4":
            failures.append(
                f"wrap off: stopped at {win._find_count.text()!r}, wanted '4 of 4'"
            )
        win._find_wrap.setChecked(True)
        win._find_text(False)
        QTimer.singleShot(400, wrap_result)

    def wrap_result() -> None:
        if win._find_count.text() != "1 of 4":
            failures.append(
                f"wrap on: went to {win._find_count.text()!r}, wanted '1 of 4'"
            )
        # Choosing a row moves the document to that hit.
        win._find_results.setCurrentRow(2)
        QTimer.singleShot(400, row_result)

    def row_result() -> None:
        if win._find_count.text() != "3 of 4":
            failures.append(
                f"clicking row 2 gave {win._find_count.text()!r}, wanted '3 of 4'"
            )
        finish()

    def finish() -> None:
        for problem in failures:
            print("  FAIL", problem)
        print(f"FIND CASES: {'PASS' if not failures else 'FAIL'} "
              f"({len(cases)} cases plus the options panel)")
        app.exit(1 if failures else 0)

    def started(ok: bool) -> None:
        if ok and not state["loaded"]:
            state["loaded"] = True
            # mermaid and highlight.js rewrite the document; search what the
            # reader would actually be looking at.
            QTimer.singleShot(1200, next_case)

    win._view.loadFinished.connect(started)
    QTimer.singleShot(40000, lambda: (print("FIND CASES: FAIL (timed out)"), app.exit(1)))
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
