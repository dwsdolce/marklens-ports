use std::path::Path;
use std::process::Command;

/// The build number: the git commit count, matching what the other two ports
/// report and what tools/gen_version_build.py stamps.
///
/// Git wins wherever it answers, which is any checkout; the stamped file is the
/// fallback for a source tarball, which has no git to ask. Ordering them the
/// other way meant a plain `cargo run` reported whatever the packaging scripts
/// last wrote - a number that could be many commits stale, and that disagreed
/// with the other two ports for no visible reason. Where both exist they agree,
/// because the packaging scripts stamp the file from the same command.
fn build_number() -> Option<String> {
    let output = Command::new("git")
        .args(["rev-list", "--count", "HEAD"])
        .current_dir("..")
        .output();
    if let Ok(output) = output {
        if output.status.success() {
            if let Ok(text) = String::from_utf8(output.stdout) {
                let count = text.trim().to_owned();
                if !count.is_empty() {
                    return Some(count);
                }
            }
        }
    }

    let stamped = std::fs::read_to_string("../build/installer_version").ok()?;
    let build = stamped.trim().rsplit('.').next()?.to_owned();
    (!build.is_empty()).then_some(build)
}

/// Copy one directory's worth of shared assets into the frontend.
///
/// Writes only when the contents differ, so untouched files keep their mtimes
/// and a rebuild does not look like a change to everything downstream.
fn copy_assets(from: &str, to: &Path, extensions: &[&str]) {
    let entries = std::fs::read_dir(from)
        .unwrap_or_else(|e| panic!("cannot read {from}: {e}"));
    std::fs::create_dir_all(to).unwrap_or_else(|e| panic!("cannot create {to:?}: {e}"));
    for entry in entries.flatten() {
        let source = entry.path();
        let wanted = source
            .extension()
            .and_then(|e| e.to_str())
            .is_some_and(|e| extensions.contains(&e));
        if !wanted {
            continue;
        }
        let dest = to.join(entry.file_name());
        if std::fs::read(&source).ok() != std::fs::read(&dest).ok() {
            std::fs::copy(&source, &dest)
                .unwrap_or_else(|e| panic!("cannot copy {source:?} -> {dest:?}: {e}"));
        }
    }
}

/// Keep the frontend's copies of the shared assets in step with shared/.
///
/// Tauri bundles whatever is in rust/frontend, so the stylesheet, highlight.js,
/// mermaid and the toolbar icons all have to exist inside it - but they belong
/// to shared/, which is where the other two ports read them from. Deriving them
/// here rather than committing a second set means the two cannot drift, and
/// there is no duplicate for someone to edit by mistake. The files are
/// gitignored for the same reason.
fn sync_shared_assets() {
    let frontend = Path::new("../frontend");
    copy_assets("../../shared/web", frontend, &["css", "js"]);
    copy_assets("../../shared/icons", &frontend.join("icons"), &["svg"]);
    println!("cargo:rerun-if-changed=../../shared/web");
    println!("cargo:rerun-if-changed=../../shared/icons");
}

fn main() {
    sync_shared_assets();

    let version = match build_number() {
        Some(build) => format!("{} ({})", env!("CARGO_PKG_VERSION"), build),
        None => env!("CARGO_PKG_VERSION").to_string(),
    };
    println!("cargo:rustc-env=MARKLENS_VERSION={version}");

    // Without these the number would be frozen at whatever the first build saw:
    // cargo only re-runs this script when something it was told to watch
    // changes.
    //
    // NOT .git/HEAD, which was the first attempt and does nothing: that file
    // names the current branch and is untouched by committing. The reflog moves
    // on every commit, and the branch ref with it.
    println!("cargo:rerun-if-changed=../build/installer_version");
    println!("cargo:rerun-if-changed=../../.git/logs/HEAD");

    tauri_build::build()
}
