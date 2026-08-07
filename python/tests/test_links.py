"""Link resolution, driven by the shared link_cases.json contract."""

from __future__ import annotations

import os

import pytest
from conftest import load

from marklens import links

_DATA = load("link_cases.json")
_DOC = _DATA["doc"]
_CASES = _DATA["cases"]


def _posix(path: str | None) -> str | None:
    """Separators as the fixture writes them.

    The fixture is language- *and* platform-neutral, so it spells paths the
    POSIX way. ``document_relative_path`` deliberately does not: it returns
    what the host OS wants, which on Windows means backslashes, because that
    is what gets handed to ``Path`` and on to the file APIs. The contract being
    pinned here is which file a link resolves to, not how the separator is
    spelled, so the separator is normalised before comparing - the same
    tolerance the render fixtures use for engine-specific HTML.
    """
    return None if path is None else path.replace(os.sep, "/")


@pytest.mark.parametrize("case", _CASES, ids=lambda c: c["href"] or "<empty>")
def test_external(case: dict) -> None:
    assert links.external_url(case["href"]) == case["external"]


@pytest.mark.parametrize("case", _CASES, ids=lambda c: c["href"] or "<empty>")
def test_resolved(case: dict) -> None:
    # Only meaningful for non-external hrefs; external ones resolve to a path
    # too but the app never uses it, so the fixture leaves `resolved` null there.
    if case["external"] is not None:
        return
    resolved = links.document_relative_path(case["href"], _DOC)
    assert _posix(resolved) == case["resolved"]