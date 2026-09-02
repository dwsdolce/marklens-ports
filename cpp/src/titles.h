#pragma once
#include <QString>

// The window-title convention, which differs by platform and is shared by all
// three ports; see shared/spec/SPEC.md. Pure logic, so it lives in the core
// rather than in the window, and the tests reach it without a webview.
namespace titles {

// Whether a title should name the document and nothing else. macOS puts the
// application's name in the menu bar, so repeating it in every title is a
// Windows convention applied in the wrong place. Windows and Linux keep it,
// because there it really is the convention - and there the document is named
// on the toolbar instead.
extern const bool kDocumentOnly;

// The application's name and version, for a title bar that has to carry it.
QString appTitle();

// Taking the convention as an argument rather than reading the platform keeps
// this testable for both conventions from either machine.
QString forDocument(const QString &document, bool documentOnly);

} // namespace titles
