//! The settings file all three ports share.
//!
//! Until now the Qt ports kept their state in `QSettings` - the Windows
//! registry, a macOS plist, an INI file on Linux - and this port kept its own
//! `recent.json` under Tauri's config directory. Two stores meant two Open
//! Recent lists and, once a starting folder was remembered, two of those as
//! well. None of `QSettings`' backends can be reached from Rust, so the common
//! ground is a plain JSON file that all three read and write.
//!
//! Location, identical in every port:
//!
//! | Windows | `%LOCALAPPDATA%\Marklens\settings.json` |
//! | macOS   | `~/Library/Preferences/Marklens/settings.json` |
//! | Linux   | `${XDG_CONFIG_HOME:-~/.config}/Marklens/settings.json` |
//!
//! which is what Qt calls `GenericConfigLocation`. Note `preference_dir` and
//! not `config_dir`: they agree on Windows and Linux and differ on macOS, where
//! `config_dir` is Application Support and the Qt ports are in Preferences.
//! Tauri's own `app_config_dir` is keyed by bundle identifier and could never
//! agree, which is why the path is derived here instead.
//!
//! Writes are read-modify-write: the file holds keys this port does not own -
//! `toolBarStyle` means nothing here - and replacing the whole document would
//! quietly discard them. Three applications can hold the file at once and the
//! last writer wins, which is why a change made in one is seen by another only
//! when that other next reads.
//!
//! `MARKLENS_SETTINGS` overrides the path, which is what the tests use.

use serde_json::{Map, Value};
use std::path::{Path, PathBuf};

pub const RECENT_KEY: &str = "recentFiles";
pub const LAST_OPEN_DIR_KEY: &str = "lastOpenDir";
pub const RECENT_MAX: usize = 10;

pub fn settings_path() -> Option<PathBuf> {
    if let Some(override_path) = std::env::var_os("MARKLENS_SETTINGS") {
        return Some(PathBuf::from(override_path));
    }
    dirs::preference_dir().map(|d| d.join("Marklens").join("settings.json"))
}

/// The whole document, or an empty one if it is missing or unreadable. A
/// corrupt file is treated as absent rather than fatal: settings are a
/// convenience, and refusing to start because a recent list will not parse
/// would be a poor trade.
pub fn load() -> Map<String, Value> {
    settings_path()
        .and_then(|p| std::fs::read_to_string(p).ok())
        .and_then(|s| serde_json::from_str::<Value>(&s).ok())
        .and_then(|v| match v {
            Value::Object(map) => Some(map),
            _ => None,
        })
        .unwrap_or_default()
}

fn save(data: &Map<String, Value>) {
    let Some(file) = settings_path() else { return };
    if let Some(dir) = file.parent() {
        let _ = std::fs::create_dir_all(dir);
    }
    if let Ok(text) = serde_json::to_string_pretty(data) {
        let _ = std::fs::write(file, text + "\n");
    }
}

/// Merge a key into the file, leaving the ones this port does not own.
fn put(key: &str, value: Value) {
    let mut data = load();
    data.insert(key.to_string(), value);
    save(&data);
}

/// The one spelling every port agrees to store. Forward slashes, because that
/// is what Qt hands back and what the C++ port works with internally; without a
/// single spelling the same document lands in the list twice on Windows.
pub fn canonical(path: &str) -> String {
    // 92 rather than a literal: a backslash written here is exactly what
    // gets eaten by the layers between an editor and this file.
    const BACKSLASH: char = 92u8 as char;
    path.replace(BACKSLASH, "/")
}

/// Comparison key for de-duplication: Windows filenames are case-insensitive,
/// so case is folded there and nowhere else.
fn dedup_key(path: &str) -> String {
    if cfg!(target_os = "windows") {
        canonical(path).to_lowercase()
    } else {
        canonical(path)
    }
}

/// Newest first, canonicalised and de-duplicated on read, so a list written by
/// an older build or by another port is cleaned up on sight.
pub fn recent_files() -> Vec<String> {
    let Some(Value::Array(raw)) = load().get(RECENT_KEY).cloned() else {
        return Vec::new();
    };
    let mut seen: Vec<String> = Vec::new();
    let mut out: Vec<String> = Vec::new();
    for entry in raw {
        let Value::String(text) = entry else { continue };
        let spelled = canonical(&text);
        let key = dedup_key(&spelled);
        if !seen.contains(&key) {
            seen.push(key);
            out.push(spelled);
        }
    }
    out
}

pub fn add_recent(path: &str) {
    let spelled = canonical(path);
    let key = dedup_key(&spelled);
    let mut recent = vec![spelled];
    for existing in recent_files() {
        if dedup_key(&existing) != key {
            recent.push(existing);
        }
    }
    recent.truncate(RECENT_MAX);
    put(RECENT_KEY, Value::Array(recent.into_iter().map(Value::String).collect()));
}

pub fn clear_recent() {
    put(RECENT_KEY, Value::Array(Vec::new()));
}

/// The folder the Open dialog should start in, or `None` for the platform's
/// own default.
pub fn last_open_dir() -> Option<String> {
    match load().get(LAST_OPEN_DIR_KEY) {
        Some(Value::String(s)) if !s.is_empty() => Some(canonical(s)),
        _ => None,
    }
}

pub fn set_last_open_dir(path: &Path) {
    put(
        LAST_OPEN_DIR_KEY,
        Value::String(canonical(&path.to_string_lossy())),
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Each test gets its own file. Without the override these would eat the
    /// recent list of whichever port ran last, and they would collide with each
    /// other, since cargo runs tests in threads sharing one environment.
    /// MARKLENS_SETTINGS is process-wide and cargo runs tests in threads, so
    /// every test that touches it takes this first.
    static ENV_LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());

    fn with_temp_settings<T>(name: &str, body: impl FnOnce() -> T) -> T {
        let file = std::env::temp_dir().join(format!("marklens-settings-{name}.json"));
        let _ = std::fs::remove_file(&file);
        let _guard = ENV_LOCK.lock().unwrap_or_else(|e| e.into_inner());
        unsafe { std::env::set_var("MARKLENS_SETTINGS", &file) };
        let out = body();
        let _ = std::fs::remove_file(&file);
        out
    }

    /// The whole point is that three ports open the same file, and the way
    /// that fails is silently. Qt's GenericConfigLocation is AppData/Local on
    /// Windows while dirs::preference_dir() is AppData/Roaming, so the Qt ports
    /// derive Roaming by hand there; this pins down our half of that.
    #[test]
    fn default_path_is_the_one_every_port_derives() {
        let _guard = ENV_LOCK.lock().unwrap_or_else(|e| e.into_inner());
        unsafe { std::env::remove_var("MARKLENS_SETTINGS") };
        let path = settings_path().expect("a config directory");
        let root = if cfg!(target_os = "windows") {
            PathBuf::from(std::env::var("LOCALAPPDATA").expect("LOCALAPPDATA"))
        } else if cfg!(target_os = "macos") {
            PathBuf::from(std::env::var("HOME").expect("HOME")).join("Library/Preferences")
        } else {
            std::env::var("XDG_CONFIG_HOME")
                .map(PathBuf::from)
                .unwrap_or_else(|_| {
                    PathBuf::from(std::env::var("HOME").expect("HOME")).join(".config")
                })
        };
        let expected = root.join("Marklens").join("settings.json");
        // Windows disagrees with itself about case between an environment
        // variable and a known-folder lookup; elsewhere the comparison is exact.
        if cfg!(target_os = "windows") {
            assert_eq!(
                path.to_string_lossy().to_lowercase(),
                expected.to_string_lossy().to_lowercase()
            );
        } else {
            assert_eq!(path, expected);
        }
    }

    /// A document in the exact shape a Qt port writes: four-space indent,
    /// forward slashes, and a toolBarStyle key this port has no use for. The
    /// sharing is the whole point, and it would fail silently, so it is pinned
    /// here rather than left to a live run of two applications.
    #[test]
    fn reads_a_file_written_by_a_qt_port() {
        with_temp_settings("qt-shaped", || {
            let written = concat!(
                "{
",
                "    \"recentFiles\": [
",
                "        \"C:/docs/OTHER.md\",
",
                "        \"C:/docs/index.md\"
",
                "    ],
",
                "    \"lastOpenDir\": \"C:/docs\",
",
                "    \"toolBarStyle\": 2
",
                "}
"
            );
            std::fs::write(settings_path().unwrap(), written).unwrap();

            assert_eq!(recent_files(), vec!["C:/docs/OTHER.md", "C:/docs/index.md"]);
            assert_eq!(last_open_dir().as_deref(), Some("C:/docs"));

            // And a write from here leaves the Qt-only key alone.
            add_recent("C:/docs/new.md");
            assert_eq!(load().get("toolBarStyle"), Some(&Value::from(2)));
        });
    }

    #[test]
    fn missing_file_reads_as_empty() {
        with_temp_settings("missing", || {
            assert!(recent_files().is_empty());
            assert_eq!(last_open_dir(), None);
        });
    }

    #[test]
    fn recent_is_newest_first_and_deduplicated() {
        with_temp_settings("dedup", || {
            add_recent("/tmp/a.md");
            add_recent("/tmp/b.md");
            add_recent("/tmp/a.md");
            assert_eq!(recent_files(), vec!["/tmp/a.md", "/tmp/b.md"]);
        });
    }

    #[test]
    fn paths_are_stored_with_forward_slashes() {
        with_temp_settings("slashes", || {
            let native = format!("C:{}tmp{}a.md", 92u8 as char, 92u8 as char);
            add_recent(&native);
            assert_eq!(recent_files()[0], "C:/tmp/a.md");
        });
    }

    #[test]
    fn recent_is_capped() {
        with_temp_settings("cap", || {
            for i in 0..RECENT_MAX + 5 {
                add_recent(&format!("/tmp/{i}.md"));
            }
            assert_eq!(recent_files().len(), RECENT_MAX);
            assert_eq!(recent_files()[0], format!("/tmp/{}.md", RECENT_MAX + 4));
        });
    }

    #[test]
    fn writes_preserve_keys_this_port_does_not_own() {
        with_temp_settings("foreign", || {
            put("toolBarStyle", Value::from(2));
            put("somethingElse", Value::from(42));
            add_recent("/tmp/a.md");
            set_last_open_dir(Path::new("/tmp"));
            let data = load();
            assert_eq!(data.get("toolBarStyle"), Some(&Value::from(2)));
            assert_eq!(data.get("somethingElse"), Some(&Value::from(42)));
            assert_eq!(last_open_dir().as_deref(), Some("/tmp"));
        });
    }
}
