"""The PySide6 viewer: a window with a web view, a toolbar, and a file watcher.

Off-sandbox, link and image handling is far simpler than the Swift original:
the page's base URL is the document's folder, so Qt resolves relative images
and links natively. We intercept navigations only to route them — external URLs
to the system browser, other documents into the viewer.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

from PySide6.QtCore import QFileSystemWatcher, QSettings, Qt, QUrl, Signal, Slot
from PySide6.QtGui import QAction, QDesktopServices, QKeySequence
from PySide6.QtWebEngineCore import QWebEnginePage, QWebEngineSettings
from PySide6.QtWebEngineWidgets import QWebEngineView
from PySide6.QtWidgets import (
    QLineEdit,
    QMainWindow,
    QMenu,
    QToolBar,
)

from . import assets, links, renderer

_MARKDOWN_SUFFIXES = {".md", ".markdown", ".mdown", ".mkd", ".txt"}

#: Shown when no document is open. The ports are ordinary single-window apps and
#: can be launched with no file, which the SwiftUI original never could - it was
#: document-based, so macOS put up the open panel instead. See shared/spec.
_EMPTY_STATE_BODY = "<p style='opacity:.6;padding:1rem'>Open a Markdown file to view it.</p>"

if sys.platform == "darwin":
    _REVEAL_TEXT = "Show in Finder"
elif sys.platform.startswith("win"):
    _REVEAL_TEXT = "Show in Explorer"
else:
    _REVEAL_TEXT = "Show in File Manager"


class _Page(QWebEnginePage):
    """Routes link clicks instead of letting the view navigate to them."""

    open_document = Signal(Path)  # a relative link to another document
    document_path: Path | None = None

    # camelCase because it overrides Qt's virtual, not because we like it. A
    # per-line N802 suppression used to sit here, but pep8-naming is not among
    # the rules this project enables, so ruff reported it as suppressing
    # nothing. Adding extend-select = ["N"] under [tool.ruff.lint] would turn
    # the check on, and the suppression would earn its place back.
    def acceptNavigationRequest(
        self, url: QUrl | str, nav_type: QWebEnginePage.NavigationType, is_main_frame: bool
    ) -> bool:
        # Qt's stubs allow str, but at runtime it hands us a QUrl; normalize so
        # the rest can rely on QUrl methods.
        if isinstance(url, str):
            url = QUrl(url)
        if nav_type != QWebEnginePage.NavigationType.NavigationTypeLinkClicked:
            return True  # our own setHtml load, form posts, etc.

        # External (http/https/mailto/…) → hand to the system browser. Qt has
        # already resolved the href against the base URL, so we classify the
        # resolved URL with the same rule the fixtures pin down.
        if links.external_url(url.toString()) is not None:
            QDesktopServices.openUrl(url)
            return False

        # Same document, just a #fragment → let the view scroll natively.
        if (
            url.hasFragment()
            and self.document_path is not None
            # Compared as paths, not strings: Qt hands back forward slashes even
            # on Windows, where str(Path) gives backslashes, so the string form
            # never matched and every in-page anchor re-rendered the document
            # from the top instead of scrolling to the heading. The C++ port
            # avoids this by storing QFileInfo::absoluteFilePath(), which is
            # already in Qt's spelling.
            and Path(url.toLocalFile()) == self.document_path
        ):
            return True

        # Another local document → open it in the viewer.
        if url.isLocalFile():
            target = Path(url.toLocalFile())
            if target.suffix.lower() in _MARKDOWN_SUFFIXES:
                self.open_document.emit(target)
                return False
            # Non-markdown local file (image, pdf) → let the OS handle it.
            QDesktopServices.openUrl(url)
            return False

        return False


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Marklens")
        self.resize(900, 720)

        self._history: list[Path] = []
        self._current: Path | None = None
        self._auto_reload = True
        self._watcher = QFileSystemWatcher(self)
        self._watcher.fileChanged.connect(self._on_file_changed)

        self._view = QWebEngineView(self)
        self._page = _Page(self._view)
        # Queued, NOT direct: open_document fires from inside the page's
        # acceptNavigationRequest, and open_path calls setHtml. Re-entering
        # QtWebEngine's navigation machinery synchronously traps (SIGTRAP), so
        # defer the load until the navigation callback has returned.
        self._page.open_document.connect(self.open_path, Qt.ConnectionType.QueuedConnection)
        self._view.setPage(self._page)
        s = self._view.settings()
        # file:// assets (styles.css etc.) are referenced absolutely from the
        # doc-folder base URL; allow local content to load them.
        s.setAttribute(QWebEngineSettings.WebAttribute.LocalContentCanAccessFileUrls, True)
        self.setCentralWidget(self._view)

        self._build_ui()
        self._render()  # nothing open yet, so this puts up the empty state

    # ── menu bar + toolbar ───────────────────────────────────────────────────

    def _build_ui(self) -> None:
        Shortcut = QKeySequence | QKeySequence.StandardKey | str

        def make(text: str, slot, shortcut: Shortcut | None = None) -> QAction:
            a = QAction(text, self)
            a.triggered.connect(slot)
            if shortcut:
                a.setShortcut(shortcut)
            return a

        open_act = make("Open…", self._open_dialog, QKeySequence.StandardKey.Open)
        reload_act = make("Reload", self._reload, QKeySequence.StandardKey.Refresh)
        pdf_act = make("Export as PDF…", self._export_pdf, "Ctrl+Shift+E")
        reveal_act = make(_REVEAL_TEXT, self._reveal)
        close_act = make("Close", self.close, QKeySequence.StandardKey.Close)

        # Reload automatically when the file changes on disk (on by default).
        # Matches the Swift app's File ▸ Auto-Reload on Change.
        auto_reload_act = QAction("Auto-Reload on Change", self)
        auto_reload_act.setCheckable(True)
        auto_reload_act.setChecked(self._auto_reload)
        auto_reload_act.toggled.connect(self._set_auto_reload)

        self._back_action = make("Back", self._go_back, "Ctrl+[")
        self._back_action.setEnabled(False)
        zoom_in = make("Zoom In", lambda: self._zoom(+1), "Ctrl+=")
        zoom_out = make("Zoom Out", lambda: self._zoom(-1), "Ctrl+-")
        zoom_reset = make("Actual Size", self._zoom_reset, "Ctrl+0")

        find_act = make("Find…", self._focus_find, QKeySequence.StandardKey.Find)
        find_next = make("Find Next", self._find_next, QKeySequence.StandardKey.FindNext)
        find_prev = make("Find Previous", self._find_prev, QKeySequence.StandardKey.FindPrevious)

        help_act = make("Marklens Help", self._show_help, QKeySequence.StandardKey.HelpContents)
        about_act = make("About Marklens", self._show_about)
        about_act.setMenuRole(QAction.MenuRole.AboutRole)  # → application menu on macOS

        # --- menu bar (filled out on macOS, unlike a toolbar-only app) ---
        bar = self.menuBar()
        file_menu = bar.addMenu("File")
        file_menu.addAction(open_act)
        # Construct explicitly parented to self: a QMenu returned by
        # addMenu(str) can have its C++ object collected out from under the
        # Python wrapper, which then raises when we later clear() it.
        self._recent_menu = QMenu("Open Recent", self)
        file_menu.addMenu(self._recent_menu)
        self._rebuild_recent_menu()
        file_menu.addActions([reload_act, auto_reload_act])
        file_menu.addSeparator()
        file_menu.addActions([pdf_act, reveal_act])
        file_menu.addSeparator()
        file_menu.addAction(close_act)
        edit_menu = bar.addMenu("Edit")
        edit_menu.addActions([find_act, find_next, find_prev])
        view_menu = bar.addMenu("View")
        view_menu.addAction(self._back_action)
        view_menu.addSeparator()
        view_menu.addActions([zoom_in, zoom_out, zoom_reset])
        window_menu = bar.addMenu("Window")
        window_menu.addAction(make("Minimize", self.showMinimized, "Ctrl+M"))
        window_menu.addAction(make("Zoom", self._toggle_zoom))
        help_menu = bar.addMenu("Help")
        help_menu.addActions([help_act, about_act])

        # --- toolbar (a subset, for quick access) ---
        tb = QToolBar("Main", self)
        tb.setMovable(False)
        self.addToolBar(tb)
        tb.addActions([open_act, self._back_action, reload_act])
        tb.addSeparator()
        tb.addActions([zoom_out, zoom_in, zoom_reset])
        tb.addSeparator()

        self._find_input = QLineEdit(self)
        self._find_input.setPlaceholderText("Find…")
        self._find_input.setMaximumWidth(200)
        self._find_input.returnPressed.connect(self._find)
        tb.addWidget(self._find_input)

        tb.addSeparator()
        tb.addActions([pdf_act, reveal_act])

    def _focus_find(self) -> None:
        self._find_input.setFocus()
        self._find_input.selectAll()

    def _toggle_zoom(self) -> None:
        self.showNormal() if self.isMaximized() else self.showMaximized()

    # ── recent files (persisted via QSettings) ───────────────────────────────

    _RECENT_KEY = "recentFiles"
    _RECENT_MAX = 10

    @staticmethod
    def _canonical_recent(path: str | Path) -> str:
        """The one spelling both Qt ports agree to store.

        This port and the C++ one share a single QSettings store - same
        organisation and application name - but reached it spelling paths
        differently: Qt hands back forward slashes (QFileDialog,
        QUrl::toLocalFile) while ``str(Path)`` is native, so on Windows the
        same document landed in the list twice. Forward slashes win because
        that is what the C++ side already works with internally.
        """
        return Path(path).as_posix()

    @staticmethod
    def _recent_key(path: str) -> str:
        """Comparison key for de-duplication.

        ``normcase`` folds both separators and case on Windows, where
        filenames are case-insensitive, and is the identity elsewhere.
        """
        return os.path.normcase(path)

    def _load_recent(self) -> list[str]:
        val = QSettings().value(self._RECENT_KEY)
        if val is None:
            return []
        raw = [val] if isinstance(val, str) else list(val)
        # Canonicalise and de-duplicate on read, so a list written by an older
        # build (or by the C++ port) is cleaned up on sight rather than needing
        # a migration step. First occurrence wins: the list is newest-first.
        seen: set[str] = set()
        recent: list[str] = []
        for entry in raw:
            canonical = self._canonical_recent(entry)
            key = self._recent_key(canonical)
            if key not in seen:
                seen.add(key)
                recent.append(canonical)
        return recent

    def _add_recent(self, path: Path) -> None:
        canonical = self._canonical_recent(path)
        key = self._recent_key(canonical)
        recent = [p for p in self._load_recent() if self._recent_key(p) != key]
        recent.insert(0, canonical)
        QSettings().setValue(self._RECENT_KEY, recent[: self._RECENT_MAX])
        self._rebuild_recent_menu()

    def _rebuild_recent_menu(self) -> None:
        self._recent_menu.clear()
        recent = self._load_recent()
        if not recent:
            empty = self._recent_menu.addAction("No Recent Documents")
            empty.setEnabled(False)
            return
        for p in recent:
            act = self._recent_menu.addAction(Path(p).name)
            act.setToolTip(p)
            act.triggered.connect(lambda _checked=False, path=p: self.open_path(Path(path)))
        self._recent_menu.addSeparator()
        self._recent_menu.addAction("Clear Menu", self._clear_recent)

    def _clear_recent(self) -> None:
        QSettings().remove(self._RECENT_KEY)
        self._rebuild_recent_menu()

    def _show_help(self) -> None:
        from PySide6.QtWidgets import QDialog, QTextBrowser, QVBoxLayout

        # Kept on self so the modeless window isn't garbage-collected.
        self._help_dialog = QDialog(self)
        self._help_dialog.setWindowTitle("Marklens Help")
        self._help_dialog.resize(580, 620)
        browser = QTextBrowser(self._help_dialog)
        browser.setOpenExternalLinks(True)
        browser.setHtml(assets.help_html())
        layout = QVBoxLayout(self._help_dialog)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(browser)
        self._help_dialog.show()

    def _show_about(self) -> None:
        from PySide6 import __version__ as pyside_version
        from PySide6.QtWidgets import QMessageBox

        from . import PROJECT_URL, UPSTREAM_URL, __version_string__

        QMessageBox.about(
            self,
            "About Marklens",
            "<h3>Marklens</h3>"
            "<p>A native Markdown viewer, Python/PySide6 port.</p>"
            f"<p>Version {__version_string__}<br>PySide6 {pyside_version}</p>"
            f"<p><a href='{PROJECT_URL}'>{PROJECT_URL}</a></p>"
            "<p>One of three ports of the same viewer &mdash; Python/PySide6, "
            "C++/Qt and Rust/Tauri &mdash; kept behaviourally identical by a "
            "shared specification.</p>"
            f"<p>A reimplementation of <a href='{UPSTREAM_URL}'>Marklens</a> "
            "by Donald Jackson.</p>"
            "<p>Licensed under the MIT License.</p>",
        )

    # ── loading ──────────────────────────────────────────────────────────────

    @Slot(Path)
    def open_path(self, path: Path, *, record_history: bool = True) -> None:
        path = path.resolve()
        if record_history and self._current is not None and self._current != path:
            self._history.append(self._current)
            self._back_action.setEnabled(True)
        self._watch(path)
        self._current = path
        self._page.document_path = path
        self._add_recent(path)
        self._render()

    def _render(self) -> None:
        if self._current is None:
            self._view.setHtml(
                renderer.page(_EMPTY_STATE_BODY, asset_base=assets.asset_base_url())
            )
            self.setWindowTitle("Marklens")
            return
        try:
            text = self._current.read_text(encoding="utf-8")
        except OSError:
            text = self._current.read_text(errors="replace")
        body = renderer.render_body(text)
        html = renderer.page(body, asset_base=assets.asset_base_url())
        base = QUrl.fromLocalFile(str(self._current.parent) + "/")
        self._view.setHtml(html, base)
        self.setWindowTitle(f"{self._current.name} — Marklens")

    def _reload(self) -> None:
        self._render()

    def _go_back(self) -> None:
        if not self._history:
            return
        previous = self._history.pop()
        self._back_action.setEnabled(bool(self._history))
        self.open_path(previous, record_history=False)

    # ── file watching (auto-reload) ──────────────────────────────────────────

    def _watch(self, path: Path) -> None:
        if self._watcher.files():
            self._watcher.removePaths(self._watcher.files())
        self._watcher.addPath(str(path))

    @Slot(str)
    def _on_file_changed(self, changed: str) -> None:
        # Atomic saves (write-temp-then-rename) drop the watch, mirroring the
        # Swift kqueue re-arm: re-add the path, then re-render.
        if Path(changed).exists() and changed not in self._watcher.files():
            self._watcher.addPath(changed)
        if self._auto_reload and self._current and str(self._current) == changed:
            self._render()

    # ── toolbar actions ──────────────────────────────────────────────────────

    def _open_dialog(self) -> None:
        from PySide6.QtWidgets import QFileDialog

        name, _ = QFileDialog.getOpenFileName(
            self, "Open Markdown", "", "Markdown (*.md *.markdown *.mdown *.mkd);;All files (*)"
        )
        if name:
            self.open_path(Path(name))

    def _zoom(self, direction: int) -> None:
        self._view.setZoomFactor(self._view.zoomFactor() * (1.1 if direction > 0 else 1 / 1.1))

    def _zoom_reset(self) -> None:
        self._view.setZoomFactor(1.0)

    def _find(self) -> None:
        self._view.findText(self._find_input.text())

    def _find_next(self) -> None:
        self._view.findText(self._find_input.text())

    def _find_prev(self) -> None:
        self._view.findText(self._find_input.text(), QWebEnginePage.FindFlag.FindBackward)

    def _set_auto_reload(self, enabled: bool) -> None:
        self._auto_reload = enabled

    def _export_pdf(self) -> None:
        if self._current is None:
            return
        from PySide6.QtWidgets import QFileDialog

        suggested = str(self._current.with_suffix(".pdf").name)
        name, _ = QFileDialog.getSaveFileName(self, "Export PDF", suggested, "PDF (*.pdf)")
        if name:
            self._page.printToPdf(name)

    def _reveal(self) -> None:
        if self._current is None:
            return
        _reveal_in_file_manager(self._current)


def _reveal_in_file_manager(path: Path) -> None:
    if sys.platform == "darwin":
        subprocess.run(["open", "-R", str(path)], check=False)
    elif sys.platform.startswith("win"):
        subprocess.run(["explorer", f"/select,{path}"], check=False)
    else:
        QDesktopServices.openUrl(QUrl.fromLocalFile(str(path.parent)))