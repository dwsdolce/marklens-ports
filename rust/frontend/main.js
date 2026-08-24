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
const findBar = document.getElementById("findbar");
const findCount = document.getElementById("find-count");
const reloadBtn = document.getElementById("reload-btn");

// Reveal is an icon now, so the platform wording lives in its tooltip.
{
  const ua = navigator.userAgent;
  const label = /Mac/.test(ua) ? "Show in Finder" : /Win/.test(ua) ? "Show in Explorer" : "Show in File Manager";
  document.getElementById("reveal-btn").title = label;
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
  reloadBtn.classList.remove("stale"); // what changed on disk is now on screen
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

// ── find ─────────────────────────────────────────────────────────────────────
//
// Matches are marked up rather than handed to window.find(). The native call is
// less code, but it reports only whether it found something - no count, no
// position - and the Qt ports and the Swift bar both show "3 of 12". Wrapping
// the hits ourselves is what makes that number available, and it also lets the
// active hit be coloured differently from the rest.

let hits = [];
let hitIndex = -1;

function clearHits() {
  for (const mark of content.querySelectorAll("mark.find-hit")) {
    const parent = mark.parentNode;
    parent.replaceChild(document.createTextNode(mark.textContent), mark);
    parent.normalize(); // rejoin the split text nodes, so a re-search sees whole words
  }
  hits = [];
  hitIndex = -1;
  findCount.textContent = "";
}

function markHits(query) {
  const needle = query.toLowerCase();
  // Collected first: wrapping a match mutates the tree the walker is walking.
  const walker = document.createTreeWalker(content, NodeFilter.SHOW_TEXT, {
    acceptNode(node) {
      if (!node.nodeValue.toLowerCase().includes(needle)) return NodeFilter.FILTER_REJECT;
      // Skip anything whose text is not prose: <script>/<style> content, and
      // the text inside a rendered mermaid diagram, where replacing nodes would
      // corrupt the SVG.
      const tag = node.parentElement?.closest("script, style, svg");
      return tag ? NodeFilter.FILTER_REJECT : NodeFilter.FILTER_ACCEPT;
    },
  });
  const targets = [];
  for (let n = walker.nextNode(); n; n = walker.nextNode()) targets.push(n);

  for (const node of targets) {
    const text = node.nodeValue;
    const lower = text.toLowerCase();
    const frag = document.createDocumentFragment();
    let at = 0;
    for (let i = lower.indexOf(needle); i !== -1; i = lower.indexOf(needle, at)) {
      if (i > at) frag.appendChild(document.createTextNode(text.slice(at, i)));
      const mark = document.createElement("mark");
      mark.className = "find-hit";
      mark.textContent = text.slice(i, i + needle.length);
      frag.appendChild(mark);
      hits.push(mark);
      at = i + needle.length;
    }
    if (at < text.length) frag.appendChild(document.createTextNode(text.slice(at)));
    node.parentNode.replaceChild(frag, node);
  }
}

function showHit(index) {
  if (!hits.length) {
    findCount.textContent = "No matches";
    return;
  }
  hits[hitIndex]?.classList.remove("active");
  hitIndex = (index + hits.length) % hits.length;
  const mark = hits[hitIndex];
  mark.classList.add("active");
  mark.scrollIntoView({ block: "center" });
  findCount.textContent = `${hitIndex + 1} of ${hits.length}`;
}

// Re-marks from scratch each time. The documents here are one screenful to a
// few hundred lines, so the simple thing is fast enough and cannot drift out of
// step with the text the way an incremental index would.
function runSearch(query) {
  clearHits();
  if (!query) return;
  markHits(query);
  showHit(0);
}

function findText(backwards) {
  const q = findInput.value;
  if (!q) {
    clearHits();
    return;
  }
  if (!hits.length) {
    runSearch(q);
    return;
  }
  showHit(hitIndex + (backwards ? -1 : 1));
}

function focusFind() {
  // ⌘F on an open bar that already has the caret puts it away; on one that does
  // not, it comes back to it. Same as the Qt ports.
  if (!findBar.hidden && document.activeElement === findInput) {
    hideFind();
    return;
  }
  findBar.hidden = false;
  findInput.focus();
  findInput.select();
}

function hideFind() {
  findBar.hidden = true;
  clearHits();
}

findInput.addEventListener("input", () => runSearch(findInput.value));
findInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") findText(e.shiftKey);
  if (e.key === "Escape") hideFind();
});
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape" && !findBar.hidden) hideFind();
});

findBar.addEventListener("click", (e) => {
  const act = e.target.closest("button")?.dataset.act;
  if (act === "find-prev") findText(true);
  if (act === "find-next") findText(false);
  if (act === "find-close") hideFind();
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
  find: focusFind,
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
// Auto-reload is off and the file moved underneath: accent the reload glyph so
// there is something to notice, and leave the decision to the reader.
listen("document-stale", (e) => {
  if (e.payload === currentDoc) reloadBtn.classList.add("stale");
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
