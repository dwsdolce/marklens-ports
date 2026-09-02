//! The window-title convention, which differs by platform and is shared by all
//! three ports; see `shared/spec/SPEC.md`.
//!
//! Pure logic, so it lives in the library rather than the binary, and the tests
//! reach it without a window. The C++ port keeps the same rule in its core for
//! the same reason.

/// Whether a title should name the document and nothing else.
///
/// macOS puts the application's name in the menu bar, so repeating it in every
/// title is a Windows convention applied in the wrong place. Windows and Linux
/// keep it, because there it really is the convention - and there the document
/// is named on the toolbar instead.
pub const DOCUMENT_ONLY: bool = cfg!(target_os = "macos");

/// The application's name, without a version.
pub const APP_NAME: &str = "Marklens Rust";

/// The application's name and version, for a title bar that has to carry it.
///
/// `MARKLENS_VERSION` comes from `build.rs`.
pub fn app_title() -> String {
    format!("{APP_NAME} {}", env!("MARKLENS_VERSION"))
}

/// The window title for a document, by platform convention.
///
/// Taking the convention as an argument rather than reading the platform keeps
/// this testable for both conventions from either machine.
pub fn for_document(document: &str, document_only: bool) -> String {
    if !document_only {
        return app_title();
    }
    if document.is_empty() {
        APP_NAME.to_owned()
    } else {
        document.to_owned()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn macos_names_only_the_document() {
        assert_eq!(for_document("index.md", true), "index.md");
    }

    #[test]
    fn elsewhere_names_the_application() {
        assert_eq!(for_document("index.md", false), app_title());
    }

    #[test]
    fn no_document_still_names_something() {
        // An empty title bar would be worse than a redundant one.
        assert_eq!(for_document("", true), APP_NAME);
        assert_eq!(for_document("", false), app_title());
    }

    #[test]
    fn the_application_title_carries_a_version() {
        assert!(app_title().starts_with("Marklens Rust "));
        assert_ne!(app_title(), "Marklens Rust ");
    }

    #[test]
    fn the_convention_matches_the_platform() {
        assert_eq!(DOCUMENT_ONLY, cfg!(target_os = "macos"));
    }
}
