#pragma once
#include <QString>

// Locate the shared web assets (styles.css, highlight.js, mermaid.js, themes),
// shared verbatim across all ports. The absolute path is baked at configure
// time via the MARKLENS_SHARED_DIR compile definition.
namespace assets {

QString webDir();       // filesystem path to shared/web
QString assetBaseUrl(); // file:// URL of shared/web, for the page shell

// The shared help document with the OS-specific "set as default" steps
// substituted in for the current platform.
QString helpHtml();

} // namespace assets
