#pragma once
#include <QString>

// Locate the shared web assets (styles.css, highlight.js, mermaid.js, themes),
// shared verbatim across all ports.
//
// Two layouts have to work. A development build reads straight out of the
// repository's shared/ directory, whose absolute path is baked in at configure
// time as MARKLENS_SHARED_DIR. A packaged build has a copy of the assets
// inside the bundle, because the repository is not there any more. sharedDir()
// prefers the bundle and falls back to the baked-in path, so the same binary
// works either way and the tests need no special casing.
namespace assets {

QString sharedDir();    // filesystem path to shared/
QString webDir();       // filesystem path to shared/web
QString assetBaseUrl(); // file:// URL of shared/web, for the page shell
QString iconPath();     // filesystem path to the application icon

// The shared help document with the OS-specific "set as default" steps
// substituted in for the current platform.
QString helpHtml();

} // namespace assets
