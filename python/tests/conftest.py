"""Load the shared, language-neutral fixtures that every port tests against."""

from __future__ import annotations

import json
from pathlib import Path

FIXTURES = Path(__file__).resolve().parents[2] / "shared" / "spec" / "fixtures"


def load(name: str) -> dict:
    return json.loads((FIXTURES / name).read_text())