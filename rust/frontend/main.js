// Frontend driver. The Rust backend renders Markdown → HTML body and owns the
// native menu; here we inject the body, resolve images to the asset protocol,
// run highlight.js + mermaid, route link clicks, and respond to menu/toolbar
// actions (menu events arrive from Rust; toolbar buttons trigger the same).
//
// Uses the global Tauri API (withGlobalTauri) so no bundler is needed.

const { invoke, convertFileSrc } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;

const content = document.getElementById("content");
const backBtn = document.getElementById("back-btn");
const findInput = document.getElementById("find-input");

// Reveal button label follows the platform, like the other ports.
{
  const ua = navigator.userAgent;
  const label = /Mac/.test(ua) ? "Show in Finder" : /Win/.test(ua) ? "Show in Explorer" : "Show in File Manager";
  document.getElementById("reveal-btn").textContent = label;
}

let currentDoc = null;
let currentFolder = null;
const history = [];

// ── rendering ────────────────────────────────────────────────────────────────

async function show(path, { recordHistory = true } = {}) {
  let result;
  try {
    result = await invoke("render_document", { path });
  } catch (e) {
    content.innerHTML = `<p style="color:#b00">Couldn't open <code>${path}</code>: ${e}</p>`;
    return;
  }
  if (recordHistory && currentDoc && currentDoc !== path) history.push(currentDoc);
  currentDoc = path;
  currentFolder = result.folder;
  backBtn.disabled = history.length === 0;

  content.innerHTML = result.body;
  resolveImages();
  highlight();
  runMermaid();

  document.title = `${filename(path)} — Marklens`;
  invoke("watch_document", { path }).catch(() => {});
}

function resolveImages() {
  for (const img of content.querySelectorAll("img")) {
    const src = img.getAttribute("src") || "";
    if (!src || /^[a-z][a-z0-9+.-]*:/i.test(src) || src.startsWith("//")) continue;
    img.src = convertFileSrc(joinPath(currentFolder, src));
  }
}
function highlight() {
  if (window.hljs) content.querySelectorAll("pre code").forEach((el) => window.hljs.highlightElement(el));
}
function runMermaid() {
  if (window.mermaid) {
    const dark = document.documentElement.dataset.theme === "dark";
    window.mermaid.initialize({ startOnLoad: false, securityLevel: "strict", theme: dark ? "dark" : "default" });
    window.mermaid.run({ querySelector: ".mermaid" });
  }
}

// ── link routing ─────────────────────────────────────────────────────────────

content.addEventListener("click", async (e) => {
  const a = e.target.closest("a[href]");
  if (!a) return;
  const href = a.getAttribute("href");
  if (href.startsWith("#")) return; // in-page anchor: native scroll
  e.preventDefault();
  const action = await invoke("follow_link", { href, doc: currentDoc });
  if (action.action === "open") show(action.path);
});

function goBack() {
  const prev = history.pop();
  backBtn.disabled = history.length === 0;
  if (prev) show(prev, { recordHistory: false });
}

// ── find (window.find in the system webview) ─────────────────────────────────

function focusFind() {
  findInput.focus();
  findInput.select();
}
function findText(backwards) {
  const q = findInput.value;
  if (q) window.find(q, false, backwards, true);
}
findInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") findText(e.shiftKey);
});

// ── actions (shared by toolbar buttons and menu events) ──────────────────────

const actions = {
  open: () => invoke("choose_file"),
  back: goBack,
  reload: () => currentDoc && show(currentDoc, { recordHistory: false }),
  "zoom-in": () => invoke("zoom_view", { direction: 1 }),
  "zoom-out": () => invoke("zoom_view", { direction: -1 }),
  "zoom-reset": () => invoke("zoom_view", { direction: 0 }),
  print: () => invoke("print_document"),
  reveal: () => invoke("reveal_document").catch(() => {}),
};

document.getElementById("toolbar").addEventListener("click", (e) => {
  const act = e.target.closest("button")?.dataset.act;
  if (act && actions[act]) actions[act]();
});

// ── help overlay ─────────────────────────────────────────────────────────────

const helpOverlay = document.getElementById("help-overlay");
document.getElementById("help-close").addEventListener("click", () => (helpOverlay.hidden = true));
helpOverlay.addEventListener("click", (e) => {
  if (e.target === helpOverlay) helpOverlay.hidden = true;
});
async function showHelp() {
  document.getElementById("help-body").innerHTML = await invoke("help_html");
  helpOverlay.hidden = false;
}

// ── menu events from Rust ────────────────────────────────────────────────────

listen("open-file", (e) => show(e.payload));
listen("reload", () => actions.reload());
listen("back", goBack);
listen("find", focusFind);
listen("find-next", () => findText(false));
listen("find-prev", () => findText(true));
listen("show-help", showHelp);
listen("document-changed", (e) => {
  if (e.payload === currentDoc) show(currentDoc, { recordHistory: false });
});

// ── helpers + startup ────────────────────────────────────────────────────────

function filename(p) {
  return p.split("/").pop();
}
function joinPath(folder, rel) {
  return folder.replace(/\/+$/, "") + "/" + rel;
}

invoke("initial_document").then((path) => {
  if (path) show(path);
  else content.innerHTML = "<p style='opacity:.6;padding:1rem'>Open a Markdown file to view it.</p>";
});
