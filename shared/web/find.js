/* Text search with options, over the document's own text.
 *
 * `QWebEnginePage::FindFlag` is two values - FindBackward and
 * FindCaseSensitively - and that is the whole enum. Whole words and regular
 * expressions cannot be configuration on top of `findText`, so they need a
 * different engine. This is it, and all three ports use it: the Qt ports drive
 * it through runJavaScript, the Rust port calls it directly. One engine rather
 * than three keeps "3 of 12" meaning the same thing everywhere, which is the
 * point of this repository.
 *
 * The Rust port already searched the DOM, because `window.find` reports
 * whether something matched but not how many, and the count had to come from
 * somewhere. It searched one text node at a time, which cannot see a phrase
 * that spans an element - "the **bold** word" is three text nodes, and
 * Chromium's own find crosses them. So the text is flattened first, searched
 * as one string, and each match mapped back to the nodes it covers.
 *
 * Flattening normalises the way a browser does, because the HTML a Markdown
 * renderer emits is full of newlines and indentation that nobody can see:
 * a run of whitespace becomes one space, and a block boundary becomes a line
 * break, so "world foo" does not match across the end of one paragraph and the
 * start of the next.
 */

(function (global) {
  "use strict";

  // How much of the surrounding text to keep either side of a hit for the
  // results list. Enough to tell two hits apart, short enough to fit a row.
  var CONTEXT = 40;

  // A document with more hits than this is not being read, it is being
  // pattern-matched; marking every one costs more than it is worth. The count
  // reported stays honest by saying it was truncated.
  var MAX_HITS = 5000;

  // Text inside these is not prose. Replacing nodes inside a rendered mermaid
  // diagram corrupts the SVG.
  var SKIP = "script,style,svg,head,noscript";

  // Crossing one of these ends a line, so a phrase cannot match across the gap
  // between two paragraphs or two table cells.
  var BLOCKS =
    "address,article,aside,blockquote,dd,details,div,dl,dt,fieldset," +
    "figcaption,figure,footer,form,h1,h2,h3,h4,h5,h6,header,hr,li,main,nav," +
    "ol,p,pre,section,table,tbody,td,tfoot,th,thead,tr,ul";

  var hits = [];      // [{ start, end, text, before, after, marks: [Element] }]
  var index = -1;     // which hit is current, or -1
  var lastError = null;

  function root() {
    return document.getElementById("content") || document.body;
  }

  /* ---- query ---------------------------------------------------------- */

  function escapeLiteral(s) {
    return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  }

  /* `(pattern, error)`, exactly one of which is null.
   *
   * Whole words are bounded with lookarounds rather than \b, which is defined
   * against the *pattern's* neighbouring character and so does the wrong thing
   * whenever a phrase starts or ends with punctuation: \b around "(tap" asks
   * for a word boundary before "(" - true in the middle of a word, false after
   * a space - and matches backwards. The lookarounds ask the question that was
   * actually meant, which is whether a word character abuts the match.
   */
  function compile(query) {
    var phrase = (query && query.phrase) || "";
    if (!phrase) return { pattern: null, error: null };

    var body = query.regex ? phrase : escapeLiteral(phrase);
    if (query.wholeWord) body = "(?<!\\w)(?:" + body + ")(?!\\w)";

    var flags = "g" + (query.caseSensitive ? "" : "i");
    try {
      return { pattern: new RegExp(body, flags), error: null };
    } catch (e) {
      // Half-typed patterns are the normal case, not an exception: somebody
      // typing "(foo" has an invalid pattern for as long as it takes to reach
      // the closing bracket. The bar reports it and finds nothing.
      return { pattern: null, error: e.message };
    }
  }

  /* ---- the text layer -------------------------------------------------- */

  function nearestBlock(node) {
    var el = node.parentElement;
    return el ? el.closest(BLOCKS) : null;
  }

  /* The document as one string, with a map back to the nodes it came from.
   *
   * `pieces` is in document order and covers the string exactly: each entry
   * says where a run of characters started in the flat text and where it came
   * from in a node. A collapsed run of whitespace is one piece a single
   * character wide in the text and however many wide in the node, which is why
   * the two lengths are tracked separately.
   */
  function flatten() {
    var walker = document.createTreeWalker(root(), NodeFilter.SHOW_TEXT, {
      acceptNode: function (node) {
        if (!node.nodeValue) return NodeFilter.FILTER_REJECT;
        var parent = node.parentElement;
        if (parent && parent.closest(SKIP)) return NodeFilter.FILTER_REJECT;
        return NodeFilter.FILTER_ACCEPT;
      },
    });

    var text = "";
    var pieces = [];
    var previousBlock = null;
    var first = true;

    for (var node = walker.nextNode(); node; node = walker.nextNode()) {
      var block = nearestBlock(node);
      if (!first && block !== previousBlock) text += "\n";
      previousBlock = block;
      first = false;

      var s = node.nodeValue;
      var i = 0;
      while (i < s.length) {
        var j = i;
        if (/\s/.test(s[i])) {
          while (j < s.length && /\s/.test(s[j])) j++;
          pieces.push({ node: node, from: i, to: j, at: text.length, width: 1 });
          text += " ";
        } else {
          while (j < s.length && !/\s/.test(s[j])) j++;
          pieces.push({ node: node, from: i, to: j, at: text.length, width: j - i });
          text += s.slice(i, j);
        }
        i = j;
      }
    }
    return { text: text, pieces: pieces };
  }

  /* Every (node, from, to) covered by the flat range [start, end).
   *
   * Merged per node, so a match lying inside one text node produces one
   * segment and becomes one <mark>.
   */
  function segments(pieces, start, end) {
    var out = [];
    for (var k = 0; k < pieces.length; k++) {
      var p = pieces[k];
      var pEnd = p.at + p.width;
      if (pEnd <= start) continue;
      if (p.at >= end) break;

      var from, to;
      if (p.width === p.to - p.from) {
        // Character for character: clip to the overlap.
        from = p.from + Math.max(0, start - p.at);
        to = p.from + Math.min(p.width, end - p.at);
      } else {
        // A collapsed whitespace run. It is one character in the text, so any
        // overlap at all takes the whole run.
        from = p.from;
        to = p.to;
      }
      if (to <= from) continue;

      var last = out.length ? out[out.length - 1] : null;
      if (last && last.node === p.node && last.to === from) last.to = to;
      else out.push({ node: p.node, from: from, to: to });
    }
    return out;
  }

  function wrap(node, from, to) {
    var range = document.createRange();
    range.setStart(node, from);
    range.setEnd(node, to);
    var mark = document.createElement("mark");
    mark.className = "find-hit";
    // The range lies inside one text node, so this never has to re-parent a
    // partially selected element - the case surroundContents refuses.
    range.surroundContents(mark);
    return mark;
  }

  /* ---- public surface -------------------------------------------------- */

  function clear() {
    var marks = root().querySelectorAll("mark.find-hit");
    for (var i = 0; i < marks.length; i++) {
      var mark = marks[i];
      var parent = mark.parentNode;
      while (mark.firstChild) parent.insertBefore(mark.firstChild, mark);
      parent.removeChild(mark);
      // Rejoin the text nodes the wrap split, so the next search sees whole
      // words rather than the fragments this one left behind.
      parent.normalize();
    }
    hits = [];
    index = -1;
    lastError = null;
  }

  function summary(truncated) {
    return {
      count: hits.length,
      index: index,
      error: lastError,
      truncated: !!truncated,
    };
  }

  /* Run a query and mark every match. Returns a summary; ask for `list()`
   * separately, since the Qt ports only need the count to fill the bar.
   */
  function search(query) {
    clear();
    var compiled = compile(query);
    if (compiled.error) {
      lastError = compiled.error;
      return summary(false);
    }
    if (!compiled.pattern) return summary(false);

    var flat = flatten();
    var found = [];
    var match;
    var truncated = false;
    compiled.pattern.lastIndex = 0;
    while ((match = compiled.pattern.exec(flat.text)) !== null) {
      // A pattern able to match nothing - "a*" - would otherwise spin here.
      if (match.index === compiled.pattern.lastIndex) compiled.pattern.lastIndex++;
      if (!match[0]) continue;
      if (found.length >= MAX_HITS) {
        truncated = true;
        break;
      }
      var start = match.index;
      var end = start + match[0].length;
      found.push({
        start: start,
        end: end,
        text: match[0],
        before: flat.text.slice(Math.max(0, start - CONTEXT), start),
        after: flat.text.slice(end, end + CONTEXT),
      });
    }

    // Marked last match first: wrapping splits the text node it lands in, and
    // every offset after the split moves. Going backwards leaves the offsets
    // still to be used untouched.
    for (var i = found.length - 1; i >= 0; i--) {
      var hit = found[i];
      var parts = segments(flat.pieces, hit.start, hit.end);
      var marks = [];
      for (var s = parts.length - 1; s >= 0; s--) {
        marks.unshift(wrap(parts[s].node, parts[s].from, parts[s].to));
      }
      hit.marks = marks;
    }

    hits = found;
    index = -1;
    return summary(truncated);
  }

  /* Make a hit current: colour it apart from the rest and scroll to it. */
  function show(wanted) {
    if (!hits.length) {
      index = -1;
      return summary(false);
    }
    if (index >= 0 && hits[index]) {
      hits[index].marks.forEach(function (m) { m.classList.remove("active"); });
    }
    index = ((wanted % hits.length) + hits.length) % hits.length;
    var marks = hits[index].marks;
    marks.forEach(function (m) { m.classList.add("active"); });
    if (marks[0]) marks[0].scrollIntoView({ block: "center" });
    return summary(false);
  }

  function step(backwards) {
    if (!hits.length) return summary(false);
    return show(index < 0 ? (backwards ? hits.length - 1 : 0)
                          : index + (backwards ? -1 : 1));
  }

  /* The results list: every hit with the text around it. Separate from
   * `search` because the Qt ports read it into a widget rather than the page,
   * and because a search that nobody is looking at should not pay for it.
   */
  function list() {
    return hits.map(function (h, i) {
      return {
        i: i,
        text: h.text,
        // Ellipses only where something was actually cut off.
        before: (h.start > CONTEXT ? "…" : "") + h.before.replace(/\n/g, " "),
        after: h.after.replace(/\n/g, " ") + (h.after.length === CONTEXT ? "…" : ""),
      };
    });
  }

  /* Whether the marks this engine made are still in the document.
   *
   * A re-render replaces every node, so the marks are gone while `hits` still
   * points at them - and stepping through detached nodes moves the counter and
   * nothing else, which looks exactly like the bar having stopped responding.
   */
  function stale() {
    if (!hits.length) return false;
    var first = hits[0].marks[0];
    return !first || !first.isConnected;
  }

  /* Search and go to the first hit. What typing in the field does. */
  function run(query) {
    var result = search(query);
    return result.count ? show(0) : result;
  }

  /* Go to the next or previous hit, searching first if there is nothing to
   * step through. The hosts call this rather than `step`, so that "what a
   * stale search does" and "what happens at the last match" are decided once
   * here instead of three times over.
   */
  function advance(query, backwards, wrap) {
    if (!query || !query.phrase) {
      clear();
      return summary(false);
    }
    if (stale() || !hits.length) return run(query);
    var next = index + (backwards ? -1 : 1);
    // Without wrapping the ends are walls, not a way round. The count still
    // reads the same, so the bar shows which end you are against.
    if (!wrap && (next < 0 || next >= hits.length)) return summary(false);
    return show(next);
  }

  global.MarklensFind = {
    run: run,
    advance: advance,
    search: search,
    show: show,
    step: step,
    list: list,
    clear: clear,
    stale: stale,
    state: function () { return summary(false); },
    CONTEXT: CONTEXT,
    MAX_HITS: MAX_HITS,
  };
})(window);
