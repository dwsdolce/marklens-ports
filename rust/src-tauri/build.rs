fn main() {
    // The build number tools/gen_version_build.py stamps, so the About box can
    // render "0.1.0 (4)" the way the other two ports do. tauri.conf.json keeps
    // the three-part semver it requires, which is why this reads the file
    // rather than the config.
    let build = std::fs::read_to_string("../build/installer_version")
        .ok()
        .and_then(|s| s.trim().rsplit('.').next().map(str::to_owned))
        .unwrap_or_default();
    let version = if build.is_empty() {
        env!("CARGO_PKG_VERSION").to_string()
    } else {
        format!("{} ({})", env!("CARGO_PKG_VERSION"), build)
    };
    println!("cargo:rustc-env=MARKLENS_VERSION={version}");
    println!("cargo:rerun-if-changed=../build/installer_version");

    tauri_build::build()
}
