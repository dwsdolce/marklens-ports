//! Works out what a clicked link points at.
//! Contract: ../../shared/spec/fixtures/link_cases.json.

use percent_encoding::percent_decode_str;
use std::path::{Component, Path, PathBuf};

/// An absolute non-file URL (https, mailto, …) for the system browser, or
/// `None` if `href` is a document-relative reference.
pub fn external_url(href: &str) -> Option<String> {
    match url::Url::parse(href) {
        Ok(u) if u.scheme() != "file" => Some(href.to_string()),
        _ => None, // relative refs fail to parse (no base) → not external
    }
}

/// Resolve a relative `href` against the folder holding `doc_path`. `None` when
/// there's nothing to resolve (empty href, or a bare `#fragment`). Fragment is
/// dropped.
pub fn document_relative_path(href: &str, doc_path: &str) -> Option<String> {
    // Keep an empty left side: "#frag" → "" before the fragment.
    let path_part = href.split('#').next().unwrap_or("");
    if path_part.is_empty() {
        return None;
    }
    // An href may be percent-encoded ("My%20Doc.md") or raw; decode handles the
    // former and leaves the latter untouched.
    let decoded = percent_decode_str(path_part).decode_utf8_lossy();

    let folder = Path::new(doc_path).parent()?;
    Some(lexically_normalize(&folder.join(decoded.as_ref())))
}

/// Collapse `.`/`..` lexically, without touching the filesystem — matching
/// Swift `standardizedFileURL` / Python `os.path.normpath` / C++
/// `lexically_normal`.
fn lexically_normalize(path: &Path) -> String {
    let mut out: Vec<Component> = Vec::new();
    for comp in path.components() {
        match comp {
            Component::CurDir => {}
            Component::ParentDir => match out.last() {
                Some(Component::Normal(_)) => {
                    out.pop();
                }
                // Can't climb above root (or an existing leading `..`); keep it
                // only if there's nothing normal to pop and we're not at root.
                Some(Component::RootDir) | None => {}
                _ => out.push(comp),
            },
            c => out.push(c),
        }
    }
    let mut pb = PathBuf::new();
    for c in out {
        pb.push(c.as_os_str());
    }
    pb.to_string_lossy().into_owned()
}
