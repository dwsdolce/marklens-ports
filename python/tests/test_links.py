"""Link resolution, driven by the shared link_cases.json contract."""

from __future__ import annotations

import pytest
from conftest import load

from marklens import links

_DATA = load("link_cases.json")
_DOC = _DATA["doc"]
_CASES = _DATA["cases"]


@pytest.mark.parametrize("case", _CASES, ids=lambda c: c["href"] or "<empty>")
def test_external(case: dict) -> None:
    assert links.external_url(case["href"]) == case["external"]


@pytest.mark.parametrize("case", _CASES, ids=lambda c: c["href"] or "<empty>")
def test_resolved(case: dict) -> None:
    # Only meaningful for non-external hrefs; external ones resolve to a path
    # too but the app never uses it, so the fixture leaves `resolved` null there.
    if case["external"] is not None:
        return
    assert links.document_relative_path(case["href"], _DOC) == case["resolved"]