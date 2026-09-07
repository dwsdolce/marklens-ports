#pragma once
#include <QString>
#include <QStringList>

// The settings file all three ports share.
//
// Until now the Qt ports kept their state in QSettings - the Windows registry,
// a macOS plist, an INI file on Linux - and the Rust port kept its own
// recent.json under Tauri's config directory. Two stores meant two Open Recent
// lists and, once a starting folder was remembered, two of those as well. None
// of QSettings' backends can be reached from Rust, so the common ground is a
// plain JSON file that all three read and write.
//
// Location, identical in every port:
//
//     Windows   %LOCALAPPDATA%\Marklens\settings.json
//     macOS     ~/Library/Preferences/Marklens/settings.json
//     Linux     ${XDG_CONFIG_HOME:-~/.config}/Marklens/settings.json
//
// which is what Qt calls GenericConfigLocation. The Rust port derives the same
// three by hand, because its own config directory is keyed by bundle
// identifier and would not agree.
//
// Writes are read-modify-write: the file holds keys this port does not own, and
// replacing the whole document would quietly discard them. MARKLENS_SETTINGS
// overrides the path, which is what the tests use.
namespace settings {

QString filePath();

// Newest first, canonicalised and de-duplicated on read.
QStringList recentFiles();
void addRecent(const QString &path);
void clearRecent();

// "" when nothing is stored, meaning "let the platform choose".
QString lastOpenDir();
void setLastOpenDir(const QString &path);

int toolBarStyle(int fallback);
void setToolBarStyle(int style);

// Forward slashes: the one spelling every port agrees to store. Exposed
// because the recent-files menu and the tests both need it.
QString canonical(const QString &path);

} // namespace settings
