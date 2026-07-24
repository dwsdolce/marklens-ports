"""Works out what a clicked link points at.

The webview's base URL is the document's folder, so relative links would
navigate the view itself. We intercept them instead: external URLs go to the
system browser, relative references open another document in the viewer.

Ported from the Swift ``LinkResolver``; the shared ``link_cases.json`` fixture
is the contract.
"""

from __future__ import annotations

import os
from urllib.parse import unquote, urlparse


def external_url(href: str) -> str | None:
    """An absolute non-file URL (https, mailto, …) for the system browser, or
    ``None`` if ``href`` is a document-relative reference."""
    parsed = urlparse(href)
    if parsed.scheme and parsed.scheme != "file":
        return href
    return None


def document_relative_path(href: str, doc_path: str) -> str | None:
    """Resolve a relative ``href`` against the folder holding ``doc_path``.

    Returns an absolute filesystem path, or ``None`` when there's nothing to
    resolve (empty href, or a bare ``#fragment``). Any fragment is dropped —
    no cross-file deep-linking yet.
    """
    # Keep an empty left side: "#frag".split("#", 1) -> ["", "frag"].
    path_part = href.split("#", 1)[0]
    if not path_part:
        return None

    # An href may be percent-encoded ("My%20Doc.md") or raw ("My Doc.md");
    # unquote handles the former and leaves the latter untouched.
    path_part = unquote(path_part)

    folder = os.path.dirname(doc_path)
    # Lexical normalization (no filesystem access), matching Swift's
    # standardizedFileURL, so "../" collapses without following symlinks.
    return os.path.normpath(os.path.join(folder, path_part))