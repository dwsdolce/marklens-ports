//! Core tests driven by the shared, language-neutral fixtures — the same
//! contract the Python and C++ ports satisfy.

use serde_json::Value;
use std::fs;
use std::path::PathBuf;

fn fixture(name: &str) -> Value {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../shared/spec/fixtures")
        .join(name);
    serde_json::from_str(&fs::read_to_string(&path).unwrap()).unwrap()
}

#[test]
fn render_cases() {
    let data = fixture("render_cases.json");
    for case in data["cases"].as_array().unwrap() {
        let name = case["name"].as_str().unwrap();
        let md = case["md"].as_str().unwrap();
        let html = marklens::renderer::render_body(md);
        for needle in case["contains"].as_array().unwrap() {
            let n = needle.as_str().unwrap();
            assert!(html.contains(n), "[{name}] expected {n:?} in:\n{html}");
        }
        for needle in case["absent"].as_array().unwrap() {
            let n = needle.as_str().unwrap();
            assert!(!html.contains(n), "[{name}] unexpected {n:?} in:\n{html}");
        }
    }
}

#[test]
fn link_cases() {
    let data = fixture("link_cases.json");
    let doc = data["doc"].as_str().unwrap();
    for case in data["cases"].as_array().unwrap() {
        let href = case["href"].as_str().unwrap();
        let external = case["external"].as_str();
        let resolved = case["resolved"].as_str();

        assert_eq!(
            marklens::links::external_url(href).as_deref(),
            external,
            "external_url({href:?})"
        );
        // resolved is only asserted for non-external hrefs (matches the fixture).
        if external.is_none() {
            assert_eq!(
                marklens::links::document_relative_path(href, doc).as_deref(),
                resolved,
                "document_relative_path({href:?})"
            );
        }
    }
}
