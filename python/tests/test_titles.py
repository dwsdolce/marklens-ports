"""The window-title convention, checked for both platforms from either one.

macOS puts the application's name in the menu bar, so a title that repeats it
is a Windows convention in the wrong place. ``title_for`` takes the convention
as an argument rather than reading ``sys.platform``, which is what lets this
run anywhere - the rule is shared by all three ports and belongs in
``shared/spec/SPEC.md``, not in whichever machine happens to run the suite.
"""

from __future__ import annotations

from marklens.app import APP_TITLE, DOCUMENT_ONLY_TITLE, title_for


def test_macos_names_only_the_document() -> None:
    assert title_for("index.md", document_only=True) == "index.md"


def test_elsewhere_names_the_application() -> None:
    assert title_for("index.md", document_only=False) == APP_TITLE


def test_no_document_still_names_something() -> None:
    # An empty title bar would be worse than a redundant one.
    assert title_for("", document_only=True) == "Marklens Python"
    assert title_for("", document_only=False) == APP_TITLE


def test_the_application_title_carries_a_version() -> None:
    assert APP_TITLE.startswith("Marklens Python ")
    assert APP_TITLE != "Marklens Python "


def test_the_convention_matches_the_platform() -> None:
    import sys

    assert DOCUMENT_ONLY_TITLE == (sys.platform == "darwin")
