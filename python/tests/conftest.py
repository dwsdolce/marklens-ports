"""Load the shared, language-neutral fixtures that every port tests against."""

from __future__ import annotations

import json
from pathlib import Path

FIXTURES = Path(__file__).resolve().parents[2] / "shared" / "spec" / "fixtures"


def load(name: str) -> dict:
    # encoding is explicit because read_text() defaults to the locale codec:
    # on Windows that is cp1252, which turns the em dash in a fixture into
    # three characters and quietly changes what the case asserts.
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))