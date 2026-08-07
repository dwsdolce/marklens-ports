"""Run the offscreen GUI checks under pytest.

QtWebEngine needs a real event loop and its own process, so each check is a
standalone script launched via subprocess (exit 0 = pass) rather than run
in-process. This keeps `pytest` as the single entry point for the whole suite.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest

TESTS_DIR = Path(__file__).resolve().parent
ENV = {
    **os.environ,
    "QT_QPA_PLATFORM": "offscreen",
    "QTWEBENGINE_CHROMIUM_FLAGS": "--disable-gpu --no-sandbox",
    "QTWEBENGINE_DISABLE_SANDBOX": "1",
    "PYTHONPATH": str(TESTS_DIR),
}


@pytest.mark.parametrize("script", ["smoke_gui.py", "nav_smoke.py"])
def test_gui_script(script: str) -> None:
    result = subprocess.run(
        [sys.executable, str(TESTS_DIR / script)],
        env=ENV,
        capture_output=True,
        text=True,
        timeout=60,
        # Explicitly not check=True: the assert below reports the child's own
        # stdout and stderr, which a CalledProcessError traceback would hide.
        check=False,
    )
    assert result.returncode == 0, f"{script} failed:\n{result.stdout}\n{result.stderr}"
