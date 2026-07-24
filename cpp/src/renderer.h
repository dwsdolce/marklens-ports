#pragma once
#include <QString>

// Markdown -> HTML, the Mermaid rewrite, and the page shell.
// Contract: ../shared/spec/fixtures/render_cases.json.
namespace renderer {

// Markdown -> HTML body (Mermaid blocks rewritten, no page shell).
QString renderBody(const QString &markdown);

// True if the document contains a ```mermaid block.
bool containsMermaid(const QString &markdown);

// Wrap a rendered body in the shared HTML shell. assetBase is an absolute
// file:// URL to shared/web; the document's own images resolve against the
// webview's base URL (its folder) instead.
QString page(const QString &body, const QString &assetBase, bool dark = false);

} // namespace renderer
