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

fn main() {
    let version = match build_number() {
        Some(build) => format!("{} ({})", env!("CARGO_PKG_VERSION"), build),
        None => env!("CARGO_PKG_VERSION").to_string(),
    };
    println!("cargo:rustc-env=MARKLENS_VERSION={version}");

    // Without these the number would be frozen at whatever the first build saw:
    // cargo only re-runs this script when something it was told to watch
    // changes. HEAD covers committing, and the stamp file covers packaging.
    println!("cargo:rerun-if-changed=../build/installer_version");
    println!("cargo:rerun-if-changed=../../.git/HEAD");

    tauri_build::build()
}
