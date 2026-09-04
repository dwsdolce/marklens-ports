// Prevent a console window on Windows release builds.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use marklens::{links, renderer, titles};
use notify::{EventKind, RecommendedWatcher, RecursiveMode, Watcher};
use serde::Serialize;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use tauri::menu::{
    AboutMetadataBuilder, CheckMenuItem, CheckMenuItemBuilder, MenuBuilder, MenuItemBuilder,
    PredefinedMenuItem, Submenu, SubmenuBuilder,
};
use tauri::{AppHandle, Emitter, Manager, State, Wry};
use tauri_plugin_dialog::DialogExt;
use tauri_plugin_opener::OpenerExt;

struct AppState {
    current: Mutex<Option<String>>, // document being shown
    initial: Mutex<Option<String>>, // file passed on the command line
    watcher: Mutex<Option<RecommendedWatcher>>,
    auto_reload: Mutex<bool>,
    zoom: Mutex<f64>,
    // Handles to the two menu pieces that change while the app runs, so each
    // can be updated on its own. Rebuilding the whole menu to change either is
    // what produced a burst of GTK accelerator warnings on every open; see
    // refresh_recent. Both are Arc-backed, so these are cheap clones of the
    // live items rather than copies.
    recent_menu: Mutex<Option<Submenu<Wry>>>,
    auto_reload_item: Mutex<Option<CheckMenuItem<Wry>>>,
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
    refresh_recent(&app);

    // The frontend sets document.title, which a Tauri webview does not
    // propagate to the native window, so the title is set from here.
    if let Some(window) = app.get_webview_window("main") {
        let _ = window.set_title(&titles::for_document(&filename(&path), titles::DOCUMENT_ONLY));
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
    Open { path: String, fragment: String },
    None,
}

#[tauri::command]
fn follow_link(app: AppHandle, href: String, doc: String) -> LinkAction {
    if let Some(url) = links::external_url(&href) {
        let _ = app.opener().open_url(url, None::<&str>);
        return LinkAction::External;
    }
    match links::document_relative_path(&href, &doc) {
        // The fragment rides along: "setup.md#windows-shells" has to open the
        // other document AND land on the heading.
        Some(path) => LinkAction::Open {
            path,
            fragment: links::fragment_of(&href),
        },
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
            if ours && changed {
                // With auto-reload off the change is still worth reporting, so
                // the toolbar can badge its reload glyph the way the Qt ports
                // and the Swift app do. Silence would leave the window showing
                // stale text with nothing to say so.
                let event = if auto { "document-changed" } else { "document-stale" };
                let _ = emit_to.emit(event, &changed_path);
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

/// Replace the contents of the Open Recent submenu from the stored list.
///
/// Split out of build_menu so the list can be refreshed without rebuilding the
/// menu bar around it. GTK warns - once per accelerator, so nine lines a time -
/// when a menu item carrying an accelerator is torn down, and rebuilding the
/// whole bar tore down every one of them. Nothing in here has an accelerator,
/// so swapping these items is silent.
fn fill_recent(app: &AppHandle, menu: &Submenu<Wry>) -> tauri::Result<()> {
    while menu.remove_at(0)?.is_some() {}

    let recent = load_recent(app);
    if recent.is_empty() {
        menu.append(
            &MenuItemBuilder::with_id("recent_none", "No Recent Documents")
                .enabled(false)
                .build(app)?,
        )?;
    } else {
        for p in &recent {
            menu.append(&MenuItemBuilder::with_id(format!("recent:{p}"), filename(p)).build(app)?)?;
        }
        menu.append(&PredefinedMenuItem::separator(app)?)?;
        menu.append(&MenuItemBuilder::with_id("recent_clear", "Clear Menu").build(app)?)?;
    }
    Ok(())
}

/// Refresh Open Recent in place, if the menu has been built yet.
fn refresh_recent(app: &AppHandle) {
    let menu = app.state::<AppState>().recent_menu.lock().unwrap().clone();
    if let Some(menu) = menu {
        let _ = fill_recent(app, &menu);
    }
}

fn build_menu(app: &AppHandle) -> tauri::Result<()> {
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

    // Built empty and filled separately, so that later refreshes go through the
    // same path as the first one.
    let recent_menu = SubmenuBuilder::new(app, "Open Recent").build()?;
    fill_recent(app, &recent_menu)?;

    let auto_item = CheckMenuItemBuilder::with_id("auto_reload", "Auto-Reload on Change")
        .checked(auto)
        .build(app)?;

    {
        let state = app.state::<AppState>();
        *state.recent_menu.lock().unwrap() = Some(recent_menu.clone());
        *state.auto_reload_item.lock().unwrap() = Some(auto_item.clone());
    }

    let file = SubmenuBuilder::new(app, "File")
        .item(&MenuItemBuilder::with_id("open", "Open…").accelerator("CmdOrCtrl+O").build(app)?)
        .item(&recent_menu)
        .item(&MenuItemBuilder::with_id("reload", "Reload").accelerator("CmdOrCtrl+R").build(app)?)
        .item(&auto_item)
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
            // Tick the existing item rather than rebuild the bar to redraw one
            // checkmark. GTK toggles it itself on click, so this is also what
            // keeps the tick honest if that ever disagrees with our state.
            let item = s.auto_reload_item.lock().unwrap().clone();
            if let Some(item) = item {
                let _ = item.set_checked(new);
            }
        }
        "recent_clear" => {
            clear_recent(app);
            refresh_recent(app);
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

/// Resolve a path given on the command line to an absolute one.
///
/// Falls back to the original string when the file cannot be resolved - a
/// missing file is the caller's problem to report, not this function's.
fn absolute(path: &str) -> String {
    let Ok(resolved) = std::fs::canonicalize(path) else {
        return path.to_owned();
    };
    let text = resolved.to_string_lossy().into_owned();
    // Windows canonicalize hands back a \\?\ verbatim path. It is correct, and
    // it also displays badly and compares unequal to every other spelling of
    // the same file, so the prefix comes off.
    text.strip_prefix(r"\\?\").map(str::to_owned).unwrap_or(text)
}

/// Open one of the folders from the document's path menu in the file manager.
#[tauri::command]
fn open_folder(app: AppHandle, path: String) {
    let _ = app.opener().open_path(path, None::<&str>);
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
    // WebKitGTK 2.42 began compositing through a DMA-BUF buffer shared with the
    // GPU. Where that handshake fails the web process dies moments after the
    // first paint, and it dies quietly: the window keeps the frame it already
    // painted, so the chrome looks perfectly normal, but no script ever runs
    // and nothing reflows. What that looks like from the outside is a document
    // area frozen at its opening size with the window resizing around it, and
    // a file that opens to a blank page - the title bar even updates, because
    // that is the native side, which is still alive.
    //
    // Virtualised GPUs are the common case: this was found on VMware's SVGA II
    // adapter, and the same fault is reported on VirtualBox and some NVIDIA
    // setups. Only Linux is affected - Windows is WebView2 and macOS is
    // WKWebView, so neither goes near this code path, which is why the same
    // frontend behaves on both.
    //
    // Disabling the DMA-BUF path falls back to a shared-memory buffer: a little
    // more work to composite, and correct everywhere. It has to be set before
    // GTK or WebKit is touched, because the web process reads it when it starts.
    //
    // A value already in the environment is left alone, and WebKit reads this
    // one by value rather than by presence, so anyone on hardware where the
    // fast path works can ask for it back with
    // WEBKIT_DISABLE_DMABUF_RENDERER=0.
    #[cfg(target_os = "linux")]
    if std::env::var_os("WEBKIT_DISABLE_DMABUF_RENDERER").is_none() {
        std::env::set_var("WEBKIT_DISABLE_DMABUF_RENDERER", "1");
    }

    // Absolute, so everything downstream sees one spelling of the path: the
    // recent list, the file watcher, and the folder that relative images
    // resolve against. The other two ports do the same - QFileInfo's
    // absoluteFilePath, Path.resolve - and without it `marklens-rust ./doc.md`
    // renders the text while every relative image resolves against a relative
    // folder the asset protocol cannot open, so they come out broken.
    let initial = std::env::args()
        .skip(1)
        .find(|a| !a.starts_with('-'))
        .map(|arg| absolute(&arg));

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState {
            current: Mutex::new(None),
            initial: Mutex::new(initial),
            watcher: Mutex::new(None),
            auto_reload: Mutex::new(true),
            zoom: Mutex::new(1.0),
            recent_menu: Mutex::new(None),
            auto_reload_item: Mutex::new(None),
        })
        .invoke_handler(tauri::generate_handler![
            initial_document,
            render_document,
            follow_link,
            watch_document,
            choose_file,
            zoom_view,
            reveal_document,
            open_folder,
            print_document,
            help_html
        ])
        .setup(|app| {
            let handle = app.handle();
            build_menu(handle)?;

            if let Some(window) = handle.get_webview_window("main") {
                let _ = window.set_title(&titles::for_document("", titles::DOCUMENT_ONLY));
            }

            // Nothing named on the command line: pick up where you left off,
            // as the Swift app does. The stored list outlives the files in it -
            // renamed, deleted, on a volume that is not mounted - so it is
            // walked until one still exists; finding none leaves the empty
            // state up. A document opened from Finder arrives later as
            // RunEvent::Opened and replaces this.
            let state = handle.state::<AppState>();
            let mut initial = state.initial.lock().unwrap();
            if initial.is_none() {
                *initial = load_recent(handle)
                    .into_iter()
                    .find(|p| std::path::Path::new(p).exists());
            }
            drop(initial);
            Ok(())
        })
        .on_menu_event(|app, event| handle_menu(app, event.id().0.as_str()))
        .build(tauri::generate_context!())
        .expect("error while building Marklens")
        // macOS does not pass a double-clicked or "Open With" document in argv.
        // It sends an Apple Event, which Tauri surfaces here as RunEvent::Opened
        // and otherwise discards - so the app comes up on its empty state and
        // the file association looks broken when it is only unhandled.
        .run(|_app, _event| {
            // RunEvent::Opened exists only on macOS and iOS - Tauri gates the
            // variant itself - so naming it unguarded stops the crate compiling
            // on Windows and Linux ("no variant named `Opened`"). The whole body
            // is behind the cfg rather than the match arm, since the bindings
            // and the emit below only mean anything where the variant exists.
            #[cfg(target_os = "macos")]
            {
            let (app, event) = (_app, _event);
            let tauri::RunEvent::Opened { urls } = event else {
                return;
            };
            let Some(path) = urls
                .iter()
                .filter_map(|url| url.to_file_path().ok())
                .next()
            else {
                return;
            };
            let path = path.to_string_lossy().into_owned();
            // Both routes, because which one lands depends on how far the
            // frontend has got. It registers its open-file listener before it
            // asks for the initial document, so an event that arrives after the
            // page is up is heard; one that arrives before it loads is not, and
            // is collected from `initial` instead. Setting both also displaces
            // the most-recent document seeded during setup, which would
            // otherwise be what a Finder-launched window rendered.
            *app.state::<AppState>().initial.lock().unwrap() = Some(path.clone());
            let _ = app.emit("open-file", path);
            }
        });
}
