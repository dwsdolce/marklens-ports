"""Markdown → HTML, driven by the shared render_cases.json contract."""

from __future__ import annotations

import pytest
from conftest import load

from marklens import renderer

_CASES = load("render_cases.json")["cases"]


@pytest.mark.parametrize("case", _CASES, ids=lambda c: c["name"])
def test_render_case(case: dict) -> None:
    html = renderer.render_body(case["md"])
    for needle in case["contains"]:
        assert needle in html, f"{case['name']}: expected {needle!r} in:\n{html}"
    for needle in case["absent"]:
        assert needle not in html, f"{case['name']}: unexpected {needle!r} in:\n{html}"