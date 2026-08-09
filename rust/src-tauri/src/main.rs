// Prevent a console window on Windows release builds.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use marklens::{links, renderer};
use notify::{EventKind, RecommendedWatcher, RecursiveMode, Watcher};
use serde::Serialize;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use tauri::menu::{
    AboutMetadataBuilder, CheckMenuItemBuilder, MenuBuilder, MenuItemBuilder, SubmenuBuilder,
};
use tauri::{AppHandle, Emitter, Manager, State};
use tauri_plugin_dialog::DialogExt;
use tauri_plugin_opener::OpenerExt;

struct AppState {
    current: Mutex<Option<String>>, // document being shown
    initial: Mutex<Option<String>>, // file passed on the command line
    watcher: Mutex<Option<RecommendedWatcher>>,
    auto_reload: Mutex<bool>,
    zoom: Mutex<f64>,
}

#[derive(Serialize)]
struct Rendered {
    body: String,
    folder: String,
}

// ── commands ────────────────────────────────────────────────────────────────

#[tauri::command]
fn initial_document(state: State<AppState>) -> Option<String> {
    state.initial.lock().unwrap().clone()
}

#[tauri::command]
fn render_document(app: AppHandle, state: State<AppState>, path: String) -> Result<Rendered, String> {
    let text = std::fs::read_to_string(&path).map_err(|e| e.to_string())?;
    let folder = Path::new(&path)
        .parent()
        .map(|p| p.to_string_lossy().into_owned())
        .unwrap_or_default();

    *state.current.lock().unwrap() = Some(path.clone());
    add_recent(&app, &path);
    let _ = build_menu(&app); // refresh Open Recent

    // The frontend sets document.title, which a Tauri webview does not
    // propagate to the native window - so without this the title bar stays at
    // the value from tauri.conf.json while the Qt ports show the filename.
    if let Some(window) = app.get_webview_window("main") {
        let _ = window.set_title(&format!("{} \u{2014} Marklens", filename(&path)));
    }

    Ok(Rendered {
        body: renderer::render_body(&text),
        folder,
    })
}

#[derive(Serialize)]
#[serde(tag = "action", rename_all = "lowercase")]
enum LinkAction {
    External,
    Open { path: String },
    None,
}

#[tauri::command]
fn follow_link(app: AppHandle, href: String, doc: String) -> LinkAction {
    if let Some(url) = links::external_url(&href) {
        let _ = app.opener().open_url(url, None::<&str>);
        return LinkAction::External;
    }
    match links::document_relative_path(&href, &doc) {
        Some(path) => LinkAction::Open { path },
        None => LinkAction::None,
    }
}

#[tauri::command]
fn watch_document(app: AppHandle, state: State<AppState>, path: String) -> Result<(), String> {
    let emit_to = app.clone();
    let changed_path = path.clone();
    let mut watcher = notify::recommended_watcher(move |res: notify::Result<notify::Event>| {
        if let Ok(event) = res {
            let ours = event.paths.iter().any(|p| p == Path::new(&changed_path));
            let changed = matches!(
                event.kind,
                EventKind::Modify(_) | EventKind::Create(_) | EventKind::Remove(_)
            );
            let auto = *emit_to.state::<AppState>().auto_reload.lock().unwrap();
            if ours && changed && auto {
                let _ = emit_to.emit("document-changed", &changed_path);
            }
        }
    })
    .map_err(|e| e.to_string())?;

    // Watch the parent dir so atomic saves (write-temp + rename) aren't missed.
    let dir = Path::new(&path).parent().ok_or("no parent dir")?;
    watcher
        .watch(dir, RecursiveMode::NonRecursive)
        .map_err(|e| e.to_string())?;
    *state.watcher.lock().unwrap() = Some(watcher);
    Ok(())
}

/// Open the file picker (from the toolbar or menu); emits `open-file` on pick.
#[tauri::command]
fn choose_file(app: AppHandle) {
    open_file_dialog(&app);
}

/// Toolbar equivalents of the View/File menu items, so both paths behave the
/// same (native webview zoom, native reveal).
#[tauri::command]
fn zoom_view(app: AppHandle, direction: i32) {
    zoom(&app, direction);
}

#[tauri::command]
fn reveal_document(app: AppHandle) {
    reveal_current(&app);
}

/// Native print dialog (→ Save as PDF). WKWebView doesn't implement JS
/// `window.print()`, so this has to go through Tauri's webview print.
#[tauri::command]
fn print_document(app: AppHandle) {
    print_current(&app);
}

/// The shared help document with the OS-specific steps substituted, embedded at
/// compile time so it works from a bundle.
#[tauri::command]
fn help_html() -> String {
    let base = include_str!("../../../shared/help.html");
    let steps = if cfg!(target_os = "macos") {
        include_str!("../../../shared/help_default_macos.html")
    } else if cfg!(target_os = "windows") {
        include_str!("../../../shared/help_default_windows.html")
    } else {
        include_str!("../../../shared/help_default_linux.html")
    };
    base.replace("<!--DEFAULT_APP_STEPS-->", steps)
}

// ── menu ────────────────────────────────────────────────────────────────────

fn reveal_label() -> &'static str {
    if cfg!(target_os = "macos") {
        "Show in Finder"
    } else if cfg!(target_os = "windows") {
        "Show in Explorer"
    } else {
        "Show in File Manager"
    }
}

fn build_menu(app: &AppHandle) -> tauri::Result<()> {
    let recent = load_recent(app);
    let auto = *app.state::<AppState>().auto_reload.lock().unwrap();

    // MARKLENS_VERSION comes from build.rs; see there for why it is not the
    // version in tauri.conf.json.
    let about = AboutMetadataBuilder::new()
        .name(Some("Marklens"))
        .version(Some(env!("MARKLENS_VERSION")))
        .comments(Some(
            "A native Markdown viewer, Rust/Tauri port. One of three ports of the \
             same viewer - Python/PySide6, C++/Qt and Rust/Tauri - kept \
             behaviourally identical by a shared specification.",
        ))
        .website(Some("https://github.com/dwsdolce/marklens-ports"))
        .website_label(Some("Project on GitHub"))
        .license(Some("MIT"))
        .credits(Some("A reimplementation of Marklens by Donald Jackson, https://github.com/donald-jackson/marklens"))
        .build();

    // An application submenu named after the app is a macOS convention: that is
    // where the platform expects About and Quit, and nowhere else has one. Qt
    // handles this for the other two ports by giving those actions a MenuRole
    // and letting the platform relocate them; Tauri has no equivalent, so the
    // menu is simply built differently per platform.
    #[cfg(target_os = "macos")]
    let app_menu = SubmenuBuilder::new(app, "Marklens")
        .about(Some(about.clone()))
        .separator()
        .quit()
        .build()?;

    let mut recent_sub = SubmenuBuilder::new(app, "Open Recent");
    if recent.is_empty() {
        recent_sub = recent_sub.item(
            &MenuItemBuilder::with_id("recent_none", "No Recent Documents")
                .enabled(false)
                .build(app)?,
        );
    } else {
        for p in &recent {
            recent_sub = recent_sub
                .item(&MenuItemBuilder::with_id(format!("recent:{p}"), filename(p)).build(app)?);
        }
        recent_sub = recent_sub
            .separator()
            .item(&MenuItemBuilder::with_id("recent_clear", "Clear Menu").build(app)?);
    }
    let recent_menu = recent_sub.build()?;

    let file = SubmenuBuilder::new(app, "File")
        .item(&MenuItemBuilder::with_id("open", "Open…").accelerator("CmdOrCtrl+O").build(app)?)
        .item(&recent_menu)
        .item(&MenuItemBuilder::with_id("reload", "Reload").accelerator("CmdOrCtrl+R").build(app)?)
        .item(&CheckMenuItemBuilder::with_id("auto_reload", "Auto-Reload on Change").checked(auto).build(app)?)
        .separator()
        .item(&MenuItemBuilder::with_id("export_pdf", "Export as PDF…").accelerator("CmdOrCtrl+Shift+E").build(app)?)
        .item(&MenuItemBuilder::with_id("reveal", reveal_label()).build(app)?)
        .separator()
        .close_window()
        .build()?;

    let edit = SubmenuBuilder::new(app, "Edit")
        .copy()
        .select_all()
        .separator()
        .item(&MenuItemBuilder::with_id("find", "Find…").accelerator("CmdOrCtrl+F").build(app)?)
        .item(&MenuItemBuilder::with_id("find_next", "Find Next").accelerator("CmdOrCtrl+G").build(app)?)
        .item(&MenuItemBuilder::with_id("find_prev", "Find Previous").accelerator("CmdOrCtrl+Shift+G").build(app)?)
        .build()?;

    let view = SubmenuBuilder::new(app, "View")
        .item(&MenuItemBuilder::with_id("back", "Back").accelerator("CmdOrCtrl+[").build(app)?)
        .separator()
        .item(&MenuItemBuilder::with_id("zoom_in", "Zoom In").accelerator("CmdOrCtrl+=").build(app)?)
        .item(&MenuItemBuilder::with_id("zoom_out", "Zoom Out").accelerator("CmdOrCtrl+-").build(app)?)
        .item(&MenuItemBuilder::with_id("zoom_reset", "Actual Size").accelerator("CmdOrCtrl+0").build(app)?)
        .build()?;

    let window = SubmenuBuilder::new(app, "Window")
        .minimize()
        .maximize()
        .build()?;

    // Off macOS, About belongs at the foot of Help - which is where the Qt
    // ports put it, and where Windows and Linux users look for it.
    // `mut` is only used by one of the two cfg branches, so whichever platform
    // this compiles for, the other branch's mutation is absent.
    #[allow(unused_mut)]
    let mut help_builder = SubmenuBuilder::new(app, "Help")
        .item(&MenuItemBuilder::with_id("help", "Marklens Help").build(app)?);
    #[cfg(not(target_os = "macos"))]
    {
        help_builder = help_builder.separator().about(Some(about));
    }
    let help = help_builder.build()?;

    // `mut` is only used by one of the two cfg branches, so whichever platform
    // this compiles for, the other branch's mutation is absent.
    #[allow(unused_mut)]
    let mut menu_builder = MenuBuilder::new(app);
    #[cfg(target_os = "macos")]
    {
        menu_builder = menu_builder.item(&app_menu);
    }
    let menu = menu_builder
        .items(&[&file, &edit, &view, &window, &help])
        .build()?;
    app.set_menu(menu)?;
    Ok(())
}

fn handle_menu(app: &AppHandle, id: &str) {
    match id {
        "open" => open_file_dialog(app),
        "reload" => emit(app, "reload"),
        "export_pdf" => print_current(app),
        "find" => emit(app, "find"),
        "find_next" => emit(app, "find-next"),
        "find_prev" => emit(app, "find-prev"),
        "back" => emit(app, "back"),
        "show_help" | "help" => emit(app, "show-help"),
        "reveal" => reveal_current(app),
        "zoom_in" => zoom(app, 1),
        "zoom_out" => zoom(app, -1),
        "zoom_reset" => zoom(app, 0),
        "auto_reload" => {
            let s = app.state::<AppState>();
            let new = !*s.auto_reload.lock().unwrap();
            *s.auto_reload.lock().unwrap() = new;
            let _ = build_menu(app);
        }
        "recent_clear" => {
            clear_recent(app);
            let _ = build_menu(app);
        }
        other if other.starts_with("recent:") => {
            let _ = app.emit("open-file", other.trim_start_matches("recent:").to_string());
        }
        _ => {}
    }
}

// ── actions ─────────────────────────────────────────────────────────────────

fn emit(app: &AppHandle, event: &str) {
    let _ = app.emit(event, ());
}

fn open_file_dialog(app: &AppHandle) {
    let app2 = app.clone();
    app.dialog()
        .file()
        .add_filter("Markdown", &["md", "markdown", "mdown", "mkd", "txt"])
        .pick_file(move |path| {
            if let Some(fp) = path {
                let _ = app2.emit("open-file", fp.to_string());
            }
        });
}

fn reveal_current(app: &AppHandle) {
    if let Some(cur) = app.state::<AppState>().current.lock().unwrap().clone() {
        let _ = app.opener().reveal_item_in_dir(cur);
    }
}

fn print_current(app: &AppHandle) {
    if let Some(win) = app.get_webview_window("main") {
        let _ = win.print();
    }
}

fn zoom(app: &AppHandle, direction: i32) {
    let s = app.state::<AppState>();
    let mut z = s.zoom.lock().unwrap();
    *z = match direction {
        1 => (*z * 1.1).min(5.0),
        -1 => (*z / 1.1).max(0.3),
        _ => 1.0,
    };
    if let Some(win) = app.get_webview_window("main") {
        let _ = win.set_zoom(*z);
    }
}

// ── recent files (persisted JSON) ────────────────────────────────────────────

fn recent_path(app: &AppHandle) -> Option<PathBuf> {
    app.path().app_config_dir().ok().map(|d| d.join("recent.json"))
}

fn load_recent(app: &AppHandle) -> Vec<String> {
    recent_path(app)
        .and_then(|p| std::fs::read_to_string(p).ok())
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or_default()
}

fn add_recent(app: &AppHandle, path: &str) {
    let mut recent = load_recent(app);
    recent.retain(|p| p != path);
    recent.insert(0, path.to_string());
    recent.truncate(10);
    if let Some(file) = recent_path(app) {
        if let Some(dir) = file.parent() {
            let _ = std::fs::create_dir_all(dir);
        }
        let _ = std::fs::write(file, serde_json::to_string(&recent).unwrap_or_default());
    }
}

fn clear_recent(app: &AppHandle) {
    if let Some(file) = recent_path(app) {
        let _ = std::fs::remove_file(file);
    }
}

fn filename(path: &str) -> String {
    Path::new(path)
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_else(|| path.to_string())
}

// ── entry ────────────────────────────────────────────────────────────────────

fn main() {
    let initial = std::env::args().skip(1).find(|a| !a.starts_with('-'));

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState {
            current: Mutex::new(None),
            initial: Mutex::new(initial),
            watcher: Mutex::new(None),
            auto_reload: Mutex::new(true),
            zoom: Mutex::new(1.0),
        })
        .invoke_handler(tauri::generate_handler![
            initial_document,
            render_document,
            follow_link,
            watch_document,
            choose_file,
            zoom_view,
            reveal_document,
            print_document,
            help_html
        ])
        .setup(|app| {
            build_menu(app.handle())?;
            Ok(())
        })
        .on_menu_event(|app, event| handle_menu(app, event.id().0.as_str()))
        .run(tauri::generate_context!())
        .expect("error while running Marklens");
}
